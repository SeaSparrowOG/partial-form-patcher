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
		bool result = _load(a_this, a_file);
		if (result && a_this) {
			auto* manager = SoulGemCache::GetSingleton();
			if (manager) {
				manager->OnSoulGemLoaded(a_this, a_file);
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

	void SoulGemCache::OnSoulGemLoaded(RE::TESSoulGem* a_soulGem, 
		RE::TESFile* a_file)
	{
		auto soulGemID = a_soulGem->formID;
		uint32_t fileOffset = 0;
		// Note, soul gems are not description forms, so this is necessary.
		if (!a_file->OpenTES(RE::NiFile::OpenMode::kReadOnly, true)) {
			logger::error("Failed to open {}."sv, a_file->GetFilename());
			return;
		}
		if (!a_file->Seek(0)) {
			logger::info("Failed to set stream start to 0 for {}"sv, a_file->GetFilename());
			a_file->CloseTES(false);
			return;
		}

		bool found = false;
		while (a_file->SeekNextForm(true) && !found) {
			if (a_file->currentform.formID != soulGemID) {
				continue;
			}
			if (a_file->GetFormType() != RE::TESSoulGem::FORMTYPE) {
				logger::error("Stream found soul gem with ID {:0X}, but {} reports it isn't a soul gem."sv, soulGemID, a_file->GetFilename());
				return;
			}
			found = true;
			fileOffset = a_file->fileOffset;
		}

		auto data = ReadSoulGemData();
		data.file = a_file;
		data.fileOffset = fileOffset;
		data.fileTextures = a_soulGem->textures;
		data.fileNumTextures = a_soulGem->numTextures;
		if (readData.contains(soulGemID)) {
			readData.at(soulGemID).push_back(std::move(data));
		}
		else {
			auto newVec = std::vector<ReadSoulGemData>();
			newVec.push_back(std::move(data));
			readData.emplace(soulGemID, std::move(newVec));
		}
	}

	void SoulGemCache::OnDataLoaded() {
		logger::info("  Processing {} soul gems..."sv, readData.size());
		for (auto& [id, data] : readData) {
			if (data.size() < 2) {
				continue;
			}

			auto* soulGem = RE::TESForm::LookupByID<RE::TESSoulGem>(id);
			if (!soulGem) {
				logger::warn("    >Warning, soul gem with FormID {:0X} is no longer a soul gem in memory. Skipping, but take note."sv, id);
				continue;
			}

			for (auto& fileData : data) {
				CompareSoulGem(soulGem, std::move(fileData));
			}
		}
		readData.clear();

		std::unordered_map<RE::FormID, CachedData> actualChanges{};
		for (auto& [id, data] : changedData) {
			if (!data.overwritten) {
				continue;
			}
			auto* soulGem = RE::TESForm::LookupByID<RE::TESSoulGem>(id);
			if (!soulGem) {
				continue;
			}

			auto* soulGemName = soulGem->GetName();
			if (strcmp(soulGemName, "") == 0) {
				soulGemName = "[No Name]";
			}
			auto soulGemEDID = Utilities::GetEditorID(soulGem);
			soulGemEDID = soulGemEDID.empty() ? "Couldn't Retrieve EditorID" : soulGemEDID;
			logger::info("    >Patching {} ({:0X} -> {})", soulGemName, id, soulGemEDID);

			if (data.audioOwner) {
				soulGem->pickupSound = data.alternatePickUpSound;
				soulGem->putdownSound = data.alternatePutDownSound;
				logger::info("      Forwarded {:0X} and {:0X} from {}."sv,
					soulGem->pickupSound ? soulGem->pickupSound->formID : 0,
					soulGem->putdownSound ? soulGem->putdownSound->formID : 0,
					data.audioOwner->GetFilename());
			}

			if (data.visualOwner) {
				soulGem->textures = data.alternateTextures;
				soulGem->numTextures = data.alternateNumTextures;
				soulGem->model = data.alternateModel;
				logger::info("      Forwarded visual data from {}."sv, data.visualOwner->GetFilename());
			}
			actualChanges.emplace(id, std::move(data));
		}
		changedData.clear();
		changedData = actualChanges;
		actualChanges.clear();
		logger::info("  Finished processing {} patches for soul gems.", changedData.size());
	}

	void SoulGemCache::CompareSoulGem(RE::TESSoulGem* a_soulGem, 
		ReadSoulGemData a_fileData)
	{
		auto* file = a_fileData.file;
		auto offset = a_fileData.fileOffset;
		auto* fileTextures = a_fileData.fileTextures;

		if (!file->OpenTES(RE::NiFile::OpenMode::kReadOnly, true)) {
			logger::error("Failed to open file {}", file->GetFilename());
			return;
		}
		if (!file->Seek(offset)) {
			logger::error("Failed to seek for {} in {}."sv, a_soulGem->GetName(), file->GetFilename());
			file->CloseTES(false);
			return;
		}
		if (file->GetFormType() != RE::TESSoulGem::FORMTYPE) {
			logger::error("Form in {} exists, but is not a soul gem."sv, file->GetFilename());
			file->CloseTES(false);
			return;
		}
		auto soulGemID = a_soulGem->GetFormID();
		if (file->currentform.formID != soulGemID) {
			logger::error("Form in {} exists and is a soul gem, but is not the stored soul gem."sv, file->GetFilename());
			file->CloseTES(false);
			return;
		}

		RE::TESObjectSTAT* fileInventoryModel = nullptr;
		RE::BSFixedString fileModelPath = "";
		RE::BGSSoundDescriptorForm* filePickupSound = nullptr;
		RE::BGSSoundDescriptorForm* filePutdownSound = nullptr;
		while (file->SeekNextSubrecord()) {

			if (Utilities::IsSubrecord(file, "INAM")) {
				RE::FormID retrieved = 0;
				if (file->ReadData(&retrieved, file->actualChunkSize)) {
					fileInventoryModel = RE::TESForm::LookupByID<RE::TESObjectSTAT>(retrieved);
				}
			}
			// Model
			else if (Utilities::IsSubrecord(file, "MODL")) {
				std::string temp(file->actualChunkSize, '\0');
				if (file->ReadData(temp.data(), temp.size())) {
					fileModelPath = temp.c_str();
				}
			}
			// Pickup
			else if (Utilities::IsSubrecord(file, "YNAM")) {
				RE::FormID retrieved = 0;
				if (file->ReadData(&retrieved, file->actualChunkSize)) {
					filePickupSound = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(retrieved);
				}
			}
			// Putdown
			else if (Utilities::IsSubrecord(file, "ZNAM")) {
				RE::FormID retrieved = 0;
				if (file->ReadData(&retrieved, file->actualChunkSize)) {
					filePutdownSound = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(retrieved);
				}
			}
		}
		file->CloseTES(false);

		if (!changedData.contains(a_soulGem->formID)) {
			auto newChangedData = CachedData();
			newChangedData.baseModel = fileModelPath.empty() ? a_soulGem->model : fileModelPath;
			newChangedData.basePickUpSound = filePickupSound;
			newChangedData.basePutDownSound = filePutdownSound;
			newChangedData.baseTextures = fileTextures;

			changedData.emplace(a_soulGem->formID, std::move(newChangedData));
			return;
		}

		auto& data = changedData.at(soulGemID);
		data.overwritten |= data.audioOwner || data.visualOwner;

		bool previousIsAudioMaster = false;
		if (data.audioOwner) {
			for (auto* master : std::span(a_fileData.file->masterPtrs, a_fileData.file->masterCount)) {
				previousIsAudioMaster |= master == data.audioOwner;
			}
		}

		bool previousIsVisualMaster = false;
		if (data.visualOwner) {
			for (auto* master : std::span(a_fileData.file->masterPtrs, a_fileData.file->masterCount)) {
				previousIsVisualMaster |= master == data.visualOwner;
			}
		}

		bool updateVisuals = false;
		if (fileModelPath != data.baseModel)
		{
			updateVisuals = true;
		}
		bool updateSounds = false;
		if ((filePickupSound && filePickupSound != data.basePickUpSound) ||
			(filePutdownSound && filePutdownSound != data.basePutDownSound))
		{
			updateSounds = true;
		}
		if (!updateVisuals && !updateSounds) {
			return;
		}

		//Updates overwritten.
		if ((updateSounds && !data.visualOwner) ||
			(updateVisuals && !data.audioOwner) ||
			(updateVisuals && updateSounds))
		{
			data.overwritten = false;
		}

		if (updateSounds) {
			data.alternatePickUpSound = filePickupSound;
			data.alternatePutDownSound = filePutdownSound;
			data.audioOwner = a_fileData.file;
		}

		if (updateVisuals) {
			data.alternateModel = fileModelPath;
			data.visualOwner = a_fileData.file;
			data.alternateTextures = a_fileData.fileTextures;
			data.alternateNumTextures = a_fileData.fileNumTextures;
		}
	}
}