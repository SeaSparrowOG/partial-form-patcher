#include "MiscPatcher.h"

#include "common/Utilities.h"
#include "Settings/INI/INISettings.h"

namespace Hooks::MiscPatcher
{
	void PerformDataLoadedOp() {
		auto* manager = MiscObjectCache::GetSingleton();
		if (manager) {
			manager->PatchMiscObjects();
		}
	}
	bool ListenForMisc() {
		logger::info("  Installing Misc Object Patcher..."sv);
		bool result = true;
		auto shouldInstall = Settings::INI::GetSetting<bool>(Settings::INI::GENERAL_MISC).value_or(false);
		if (!shouldInstall) {
			logger::info("    >User opted to not install the patcher."sv);
			return result;
		}
		result &= Misc::HookTESObjectMISC();
		return result;
	}

	bool Misc::HookTESObjectMISC() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESObjectMISC::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, LoadMiscObject);
		return true;
	}

	inline bool Misc::LoadMiscObject(RE::TESObjectMISC* a_this, RE::TESFile* a_file) {
		auto* manager = MiscObjectCache::GetSingleton();
		if (_loadedAll) {
			auto response = _load(a_this, a_file);
			if (manager) {
				manager->ReapplyChanges(a_this);
			}
			return response;
		}

		auto* duplicate = a_file ? a_file->Duplicate() : nullptr;
		bool result = _load(a_this, a_file);
		if (result && a_this && duplicate && manager) {
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
			manager->OnMiscObjectLoaded(a_this, duplicate);
			duplicate->CloseTES(true);
		}
		return result;
	}

	void MiscObjectCache::OnMiscObjectLoaded(RE::TESObjectMISC* a_obj,
		RE::TESFile* a_file)
	{
		auto formID = a_obj->formID;
		std::string_view fileName = a_file->fileName;
		RE::BSFixedString fileModel = "";
		RE::FormID filePickupSound = 0;
		RE::FormID filePutdownSound = 0;

		while (a_file->SeekNextSubrecord()) {
			// Pickup
			if (Utilities::IsSubrecord(a_file, "YNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					filePickupSound = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// Putdown
			else if (Utilities::IsSubrecord(a_file, "ZNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					filePutdownSound = Utilities::GetAbsoluteFormID(retrieved, a_file);
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

	void MiscObjectCache::ReapplyChanges(RE::TESObjectMISC* a_obj)
	{
		auto formID = a_obj->formID;
		if (!readData.contains(formID)) {
			return;
		}
		auto& data = readData.at(formID);
		if (!data.overwritten) {
			return;
		}
		bool patchedAudio = false;
		bool patchedVisuals = false;
		if (!data.audioOwner.empty()) {
			auto* putDown = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altPutDownSound);
			auto* pickUp = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altPickUpSound);
			bool needsPatch = false;
			needsPatch |= (putDown != a_obj->putdownSound) && putDown;
			needsPatch |= (pickUp != a_obj->pickupSound) && pickUp;
			if (needsPatch) {
				a_obj->pickupSound = pickUp;
				a_obj->putdownSound = putDown;
			}
			patchedAudio |= needsPatch;
		}
		if (!data.visualOwner.empty()) {
			bool needsPatch = false;
			needsPatch |= (data.altModel != a_obj->model) && !data.altModel.empty();
			patchedVisuals |= needsPatch;
			if (needsPatch) {
				a_obj->SetModel(data.altModel.c_str());
			}
		}
	}

	void MiscObjectCache::PatchMiscObjects()
	{
		logger::info("Patching {} Misc Objects..."sv, readData.size());

		std::unordered_map<RE::FormID, ReadData> filteredData;
		for (auto& [id, data] : readData) {
			if (!data.overwritten) {
				continue;
			}
			auto* obj = RE::TESForm::LookupByID<RE::TESObjectMISC>(id);
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
				}
			}
			if (patchedAudio || patchedVisuals) {
				filteredData.emplace(id, data);
				logger::info("  >Patched weapon {} at {:0X}. Changes:"sv, obj->GetName(), id);
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
		Misc::_loadedAll = true;
		logger::info("Finished; Applied {} patches."sv, readData.size());
	}
}