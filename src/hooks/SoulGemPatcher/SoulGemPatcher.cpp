#include "SoulGemPatcher.h"

#include "common/Utilities.h"
#include "Settings/INI/INISettings.h"

namespace Hooks::SoulGemPatcher
{
	bool ListenForSoulGems() {
		logger::info("  Installing Soul Gem Patcher..."sv);
		bool result = true;
		auto shouldInstall = Settings::INI::GetSetting<bool>(Settings::INI::GENERAL_SLGM).value_or(false);
		if (!shouldInstall) {
			logger::info("    >User opted to not install the patcher."sv);
			return result;
		}
		result &= SoulGem::HookSoulGem();
		return result;
	}

	bool SoulGem::HookSoulGem() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESSoulGem::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, LoadSoulGemFromFile);
		return true;
	}

	inline bool SoulGem::LoadSoulGemFromFile(RE::TESSoulGem* a_this,
		RE::TESFile* a_file)
	{
		auto* duplicate = a_file ? a_file->Duplicate() : nullptr;
		bool result = _load(a_this, a_file);
		if (result && a_this && duplicate) {
			auto* manager = SoulGemCache::GetSingleton();
			if (manager) {
				if (!duplicate->Seek(0)) {
					SKSE::stl::report_and_fail("Failed to seek 0"sv);
				}
				bool found = false;
				auto formID = a_this->formID;
				while (!found && duplicate->SeekNextForm(true)) {
					if (duplicate->currentform.formID != formID) {
						continue;
					}
					found = true;
				}
				manager->OnSoulGemLoaded(a_this, duplicate);
			}
		}
		return result;
	}

	void PerformDataLoadedOp() {
		auto* manager = SoulGemCache::GetSingleton();
		if (manager) {
			manager->OnDataLoaded();
		}
	}

	void Hooks::SoulGemPatcher::SoulGemCache::OnSoulGemLoaded(RE::TESSoulGem* a_obj,
		RE::TESFile* a_file)
	{
		auto formID = a_obj->formID;
		std::string_view fileName = a_file->fileName;
		RE::BSFixedString fileModel = "";
		RE::FormID filePickupSound = 0;
		RE::FormID filePutdownSound = 0;

		uint16_t lightToRemove = 0;
		uint8_t normalToRemove = 0;
		for (const auto* master : std::span(a_file->masterPtrs, a_file->masterCount)) {
			if (master->compileIndex == 0xFF && master->IsLight()) {
				++lightToRemove;
			}
			else {
				++normalToRemove;
			}
		}

		auto compileIndex = std::clamp((uint8_t)(a_file->compileIndex - normalToRemove),
			(uint8_t)0,
			(uint8_t)std::numeric_limits<uint8_t>::max())
			<< 24;
		if (a_file->compileIndex == 0xFE && a_file->IsLight()) {
			compileIndex += std::clamp((uint16_t)(a_file->smallFileCompileIndex - lightToRemove),
				(uint16_t)0,
				(uint16_t)std::numeric_limits<uint16_t>::max())
				<< 12;
		}

		while (a_file->SeekNextSubrecord()) {
			// Pickup
			if (Utilities::IsSubrecord(a_file, "YNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					filePickupSound = retrieved + compileIndex;
				}
			}
			// Putdown
			else if (Utilities::IsSubrecord(a_file, "ZNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					filePutdownSound = retrieved + compileIndex;
				}
			}
			// Model
			else if (Utilities::IsSubrecord(a_file, "MODL")) {
				std::string temp(a_file->actualChunkSize, '\0');
				if (a_file->ReadData(temp.data(), temp.size())) {
					fileModel = temp.c_str();
				}
			}
		}

		if (!readData.contains(formID)) {
			auto newData = ReadData();
			newData.baseModel = fileModel;
			newData.basePickUpSound = filePickupSound;
			newData.basePutDownSound = filePutdownSound;
			readData.emplace(formID, std::move(newData));
			return;
		}

		auto& data = readData.at(formID);
		data.overwritten |= data.holdsData;

		auto& masters = a_file->masters;
		auto end = masters.end();
		bool isOverwritingMasterVisuals = false;
		bool isOverwritingMasterAudio = false;
		for (auto it = masters.begin(); (!isOverwritingMasterVisuals || !isOverwritingMasterAudio) && it != end; ++it) {
			if (!*it) { // Should never happen, these are file names that REQUIRE a name.
				continue;
			}
			auto name = std::string((*it));
			isOverwritingMasterVisuals = name == data.visualOwner;
			isOverwritingMasterAudio = name == data.audioOwner;
		}

		bool overwritesBaseTextures = fileModel != data.baseModel;
		bool overwritesBaseAudio = filePickupSound != data.basePickUpSound;
		overwritesBaseAudio |= filePutdownSound != data.basePutDownSound;

		if (isOverwritingMasterVisuals || overwritesBaseTextures)
		{
			data.altModel = fileModel;
			data.visualOwner = fileName;
			data.holdsData = true;
		}
		if (isOverwritingMasterAudio || overwritesBaseAudio)
		{
			data.altPickUpSound = filePickupSound;
			data.altPutDownSound = filePutdownSound;
			data.audioOwner = fileName;
			data.holdsData = true;
		}
	}

	void SoulGemCache::OnDataLoaded() {
		logger::info("Patching {} Soul Gems..."sv, readData.size());
		std::unordered_map<RE::FormID, ReadData> filteredData;
		for (auto& [id, data] : readData) {
			if (!data.overwritten) {
				continue;
			}
			auto* obj = RE::TESForm::LookupByID<RE::TESSoulGem>(id);
			if (!obj) {
				logger::error("  >Deleted record at {:0X}."sv, id);
				continue;
			}

			bool patchedAudio = false;
			bool patchedVisuals = false;
			if (!data.audioOwner.empty()) {
				auto* putDown = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altPutDownSound);
				auto* pickUp = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altPickUpSound);

				bool needsPatch = false;
				needsPatch |= (putDown != obj->putdownSound) && putDown;
				needsPatch |= (pickUp != obj->pickupSound) && pickUp;

				if (needsPatch) {
					obj->pickupSound = pickUp;
					obj->putdownSound = putDown;
				}
				patchedAudio |= needsPatch;
			}
			if (!data.visualOwner.empty()) {
				bool needsPatch = false;
				needsPatch |= (data.altModel != obj->model) && !data.altModel.empty();
				patchedVisuals |= needsPatch;
				if (needsPatch) {
					obj->SetModel(data.altModel.c_str());
					auto* newID = new RE::BSResource::ID();
				}
			}
			if (patchedAudio || patchedVisuals) {
				filteredData.emplace(id, data);

				logger::info("  >Patched Soul Gem {} at {:0X}. Changes:"sv, obj->GetName(), id);
				if (patchedVisuals) {
					logger::info("    -Visuals from {}"sv, data.visualOwner);
					logger::info("      >Model: {}"sv, obj->model.c_str());
				}
				if (patchedAudio) {
					logger::info("    -Audio from {}"sv, data.audioOwner);
					logger::info("      >Pickup: {:0X}"sv, obj->pickupSound ? obj->pickupSound->formID : 0);
					logger::info("      >Putdown: {:0X}"sv, obj->putdownSound ? obj->putdownSound->formID : 0);
				}
			}
		}
		readData = std::move(filteredData);
		logger::info("Finished; Applied {} patches."sv, readData.size());
	}
}