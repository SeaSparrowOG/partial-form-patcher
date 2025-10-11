#include "ArmorAddonPatcher.h"

#include "common/Utilities.h"
#include "Settings/INI/INISettings.h"

namespace Hooks::ArmorAddonPatcher
{
	bool ListenForArmorAddons() {
		logger::info("  Installing Armor Addon Patcher..."sv);
		bool result = true;
		auto shouldInstall = Settings::INI::GetSetting<bool>(Settings::INI::GENERAL_ARMA).value_or(false);
		if (!shouldInstall) {
			logger::info("    >User opted to not install Armor Addon patcher."sv);
			return result;
		}
		result &= ArmorAddon::HookTESObjectARMA();
		return result;
	}

	void PerformDataLoadedOp()
	{
		auto* manager = ArmorAddonCache::GetSingleton();
		if (manager) {
			manager->OnDataLoaded();
		}
	}

	bool ArmorAddon::HookTESObjectARMA() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESObjectARMA::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, LoadAddonFromFile);
		return true;
	}

	inline bool ArmorAddon::LoadAddonFromFile(RE::TESObjectARMA* a_this, RE::TESFile* a_file) {
		auto* manager = ArmorAddonCache::GetSingleton();
		if (_loadedAll) {
			auto response = _load(a_this, a_file);
			if (manager) {
				manager->ReApplyChanges(a_this);
			}
			return response;
		}

		auto* duplicate = a_file ? a_file->Duplicate() : nullptr;
		bool result = _load(a_this, a_file);
		if (result && a_this && duplicate && manager) {
			bool found = false;
			auto formID = a_this->formID;
			while (!found && duplicate->SeekNextForm(true)) {
				if (duplicate->currentform.formID != formID) {
					continue;
				}
				found = true;
			}
			if (!found) {
				return result;
			}
			manager->OnAddonLoaded(a_this, duplicate);
			duplicate->CloseTES(true);
		}
		return result;
	}

	void ArmorAddonCache::OnAddonLoaded(RE::TESObjectARMA* a_obj,
		RE::TESFile* a_file)
	{
		auto formID = a_obj->formID;
		RE::BSFixedString fileMaleBipedModel = 0;
		RE::BSFixedString fileFemaleBipedModel = 0;
		RE::BSFixedString fileMaleFirstPersonBipedModel = 0;
		RE::BSFixedString fileFemaleFirstPersonBipedModel = 0;
		RE::FormID fileMaleSkinTexture = 0;
		RE::FormID fileFemaleSkinTexture = 0;
		RE::FormID fileMaleSkinTextureSwapList = 0;
		RE::FormID fileFemaleSkinTextureSwapList = 0;
		RE::FormID fileFootstepSet = 0;

		while (a_file->SeekNextSubrecord()) {
			if (Utilities::IsSubrecord(a_file, "MOD2")) {
				std::string temp(a_file->actualChunkSize, '\0');
				if (a_file->ReadData(temp.data(), a_file->actualChunkSize)) {
					fileMaleBipedModel = temp.c_str();
				}
			}
			else if (Utilities::IsSubrecord(a_file, "MOD3")) {
				std::string temp(a_file->actualChunkSize, '\0');
				if (a_file->ReadData(temp.data(), a_file->actualChunkSize)) {
					fileFemaleBipedModel = temp.c_str();
				}
			}
			else if (Utilities::IsSubrecord(a_file, "MOD4")) {
				std::string temp(a_file->actualChunkSize, '\0');
				if (a_file->ReadData(temp.data(), a_file->actualChunkSize)) {
					fileMaleFirstPersonBipedModel = temp.c_str();
				}
			}
			else if (Utilities::IsSubrecord(a_file, "MOD5")) {
				std::string temp(a_file->actualChunkSize, '\0');
				if (a_file->ReadData(temp.data(), a_file->actualChunkSize)) {
					fileFemaleFirstPersonBipedModel = temp.c_str();
				}
			}
			else if (Utilities::IsSubrecord(a_file, "NAM0")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileMaleSkinTexture = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			else if (Utilities::IsSubrecord(a_file, "NAM1")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileFemaleSkinTexture = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			else if (Utilities::IsSubrecord(a_file, "NAM2")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileMaleSkinTextureSwapList = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			else if (Utilities::IsSubrecord(a_file, "NAM3")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileFemaleSkinTextureSwapList = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			else if (Utilities::IsSubrecord(a_file, "SNDD")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileFootstepSet = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
		}
		
		if (!readData.contains(formID)) {
			auto newData = ReadData();
			newData.baseMaleBipedMode = fileMaleFirstPersonBipedModel;
			newData.baseFemaleBipedMode = fileFemaleFirstPersonBipedModel;
			newData.baseMaleFirstPersonBipedModel = fileMaleFirstPersonBipedModel;
			newData.baseFemaleFirstPersonBipedModel = fileFemaleFirstPersonBipedModel;
			newData.baseMaleSkinTexture = fileMaleSkinTexture;
			newData.baseFemaleSkinTexture = fileFemaleSkinTexture;
			newData.baseMaleSkinTextureSwapList = fileMaleSkinTextureSwapList;
			newData.baseFemaleSkinTextureSwapList = fileFemaleSkinTextureSwapList;
			newData.baseFootstepSet = fileFootstepSet;
			readData.emplace(formID, std::move(newData));
			return;
		}

		auto& data = readData.at(formID);
		data.overwritten = true;

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

		bool overwritesBaseTextures = false;
		overwritesBaseTextures |= fileMaleBipedModel != data.baseMaleBipedMode;
		overwritesBaseTextures |= fileFemaleBipedModel != data.baseFemaleBipedMode;
		overwritesBaseTextures |= fileMaleFirstPersonBipedModel != data.baseMaleFirstPersonBipedModel;
		overwritesBaseTextures |= fileFemaleFirstPersonBipedModel != data.baseFemaleFirstPersonBipedModel;
		overwritesBaseTextures |= fileMaleSkinTexture != data.baseMaleSkinTexture;
		overwritesBaseTextures |= fileFemaleSkinTexture != data.baseFemaleSkinTexture;
		overwritesBaseTextures |= fileMaleSkinTextureSwapList != data.baseMaleSkinTextureSwapList;
		overwritesBaseTextures |= fileFemaleSkinTextureSwapList != data.baseFemaleSkinTextureSwapList;
		
		bool overwritesBaseAudio = fileFootstepSet != data.baseFootstepSet;

		if (isOverwritingMasterVisuals || overwritesBaseTextures)
		{
			data.altFemaleBipedModel = fileFemaleBipedModel;
			data.altMaleBipedModel = fileMaleBipedModel;
			data.altFemaleFirstPersonBipedModel = fileFemaleFirstPersonBipedModel;
			data.altMaleFirstPersonBipedModel = fileMaleFirstPersonBipedModel;
			data.altFemaleSkinTexture = fileFemaleSkinTexture;
			data.altMaleSkinTexture = fileMaleSkinTexture;
			data.altFemaleSkinTextureSwapList = fileFemaleSkinTextureSwapList;
			data.altMaleSkinTextureSwapList = fileMaleSkinTextureSwapList;
		}
		if (isOverwritingMasterAudio || overwritesBaseAudio)
		{
			data.altFootstepSet = fileFootstepSet;
		}
	}

	void ArmorAddonCache::ReApplyChanges(RE::TESObjectARMA* a_obj)
	{
		auto id = a_obj->formID;
		if (!readData.contains(id)) {
			return;
		}
		auto& data = readData.at(id);
		if (!data.overwritten) {
			return;
		}
		if (!data.audioOwner.empty()) {
			auto* footstepSet = RE::TESForm::LookupByID<RE::BGSFootstepSet>(data.altFootstepSet);
			bool needsPatch = footstepSet && (footstepSet != a_obj->footstepSet);
			if (needsPatch) {
				a_obj->footstepSet = footstepSet;
			}
		}
		if (!data.visualOwner.empty()) {
			bool needsPatch = false;
			needsPatch |= !data.altMaleBipedModel.empty() && (data.altMaleBipedModel != a_obj->bipedModels[0].model);
			needsPatch |= !data.altFemaleBipedModel.empty() && (data.altFemaleBipedModel != a_obj->bipedModels[1].model);
			needsPatch |= !data.altMaleFirstPersonBipedModel.empty() && (data.altMaleFirstPersonBipedModel != a_obj->bipedModel1stPersons[0].model);
			needsPatch |= !data.altFemaleFirstPersonBipedModel.empty() && (data.altFemaleFirstPersonBipedModel != a_obj->bipedModel1stPersons[1].model);
			needsPatch |= data.altMaleSkinTexture && (data.altMaleSkinTexture != (a_obj->skinTextures[0] ? a_obj->skinTextures[0]->formID : 0));
			needsPatch |= data.altFemaleSkinTexture && (data.altFemaleSkinTexture != (a_obj->skinTextures[1] ? a_obj->skinTextures[1]->formID : 0));
			needsPatch |= data.altMaleSkinTextureSwapList && (data.altMaleSkinTextureSwapList != (a_obj->skinTextureSwapLists[0] ? a_obj->skinTextureSwapLists[0]->formID : 0));
			needsPatch |= data.altFemaleSkinTextureSwapList && (data.altFemaleSkinTextureSwapList != (a_obj->skinTextureSwapLists[1] ? a_obj->skinTextureSwapLists[1]->formID : 0));
			if (needsPatch) {
				if (!data.altMaleBipedModel.empty()) {
					a_obj->bipedModels[0].model = data.altMaleBipedModel;
				}
				if (!data.altFemaleBipedModel.empty()) {
					a_obj->bipedModels[1].model = data.altFemaleBipedModel;
				}
				if (!data.altMaleFirstPersonBipedModel.empty()) {
					a_obj->bipedModel1stPersons[0].model = data.altMaleFirstPersonBipedModel;
				}
				if (!data.altFemaleFirstPersonBipedModel.empty()) {
					a_obj->bipedModel1stPersons[1].model = data.altFemaleFirstPersonBipedModel;
				}
				if (data.altMaleSkinTexture) {
					auto* tex = RE::TESForm::LookupByID<RE::BGSTextureSet>(data.altMaleSkinTexture);
					a_obj->skinTextures[0] = tex;
				}
				if (data.altFemaleSkinTexture) {
					auto* tex = RE::TESForm::LookupByID<RE::BGSTextureSet>(data.altFemaleSkinTexture);
					a_obj->skinTextures[1] = tex;
				}
				if (data.altMaleSkinTextureSwapList) {
					auto* list = RE::TESForm::LookupByID<RE::BGSListForm>(data.altMaleSkinTextureSwapList);
					a_obj->skinTextureSwapLists[0] = list;
				}
				if (data.altFemaleSkinTextureSwapList) {
					auto* list = RE::TESForm::LookupByID<RE::BGSListForm>(data.altFemaleSkinTextureSwapList);
					a_obj->skinTextureSwapLists[1] = list;
				}
			}
		}
	}

	void ArmorAddonCache::OnDataLoaded()
	{
		logger::info("Patching {} Armor Addons..."sv, readData.size());

		std::unordered_map<RE::FormID, ReadData> filteredData;
		for (auto& [id, data] : readData) {
			if (!data.overwritten) {
				continue;
			}
			auto* obj = RE::TESForm::LookupByID<RE::TESObjectARMA>(id);
			if (!obj) {
				logger::error("  >Deleted record at {:0X}."sv, id);
				continue;
			}

			bool patchedAudio = false;
			bool patchedVisuals = false;
			if (!data.audioOwner.empty()) {
				auto* footstepSet = RE::TESForm::LookupByID<RE::BGSFootstepSet>(data.altFootstepSet);

				bool needsPatch = footstepSet && (footstepSet != obj->footstepSet);
				if (needsPatch) {
					obj->footstepSet = footstepSet;
				}
				patchedAudio |= needsPatch;
			}
			if (!data.visualOwner.empty()) {
				bool needsPatch = false;
				needsPatch |= !data.altMaleBipedModel.empty() && (data.altMaleBipedModel != obj->bipedModels[0].model);
				needsPatch |= !data.altFemaleBipedModel.empty() && (data.altFemaleBipedModel != obj->bipedModels[1].model);
				needsPatch |= !data.altMaleFirstPersonBipedModel.empty() && (data.altMaleFirstPersonBipedModel != obj->bipedModel1stPersons[0].model);
				needsPatch |= !data.altFemaleFirstPersonBipedModel.empty() && (data.altFemaleFirstPersonBipedModel != obj->bipedModel1stPersons[1].model);
				needsPatch |= data.altMaleSkinTexture && (data.altMaleSkinTexture != (obj->skinTextures[0] ? obj->skinTextures[0]->formID : 0));
				needsPatch |= data.altFemaleSkinTexture && (data.altFemaleSkinTexture != (obj->skinTextures[1] ? obj->skinTextures[1]->formID : 0));
				needsPatch |= data.altMaleSkinTextureSwapList && (data.altMaleSkinTextureSwapList != (obj->skinTextureSwapLists[0] ? obj->skinTextureSwapLists[0]->formID : 0));
				needsPatch |= data.altFemaleSkinTextureSwapList && (data.altFemaleSkinTextureSwapList != (obj->skinTextureSwapLists[1] ? obj->skinTextureSwapLists[1]->formID : 0));

				if (needsPatch) {
					if (!data.altMaleBipedModel.empty()) {
						obj->bipedModels[0].model = data.altMaleBipedModel;
					}
					if (!data.altFemaleBipedModel.empty()) {
						obj->bipedModels[1].model = data.altFemaleBipedModel;
					}
					if (!data.altMaleFirstPersonBipedModel.empty()) {
						obj->bipedModel1stPersons[0].model = data.altMaleFirstPersonBipedModel;
					}
					if (!data.altFemaleFirstPersonBipedModel.empty()) {
						obj->bipedModel1stPersons[1].model = data.altFemaleFirstPersonBipedModel;
					}
					if (data.altMaleSkinTexture) {
						auto* tex = RE::TESForm::LookupByID<RE::BGSTextureSet>(data.altMaleSkinTexture);
						obj->skinTextures[0] = tex;
					}
					if (data.altFemaleSkinTexture) {
						auto* tex = RE::TESForm::LookupByID<RE::BGSTextureSet>(data.altFemaleSkinTexture);
						obj->skinTextures[1] = tex;
					}
					if (data.altMaleSkinTextureSwapList) {
						auto* list = RE::TESForm::LookupByID<RE::BGSListForm>(data.altMaleSkinTextureSwapList);
						obj->skinTextureSwapLists[0] = list;
					}
					if (data.altFemaleSkinTextureSwapList) {
						auto* list = RE::TESForm::LookupByID<RE::BGSListForm>(data.altFemaleSkinTextureSwapList);
						obj->skinTextureSwapLists[1] = list;
					}
				}
			}
			if (patchedAudio || patchedVisuals) {
				filteredData.emplace(id, data);
				logger::info("  >Patched book {} at {:0X}. Changes:"sv, obj->GetName(), id);
				if (patchedVisuals) {
					logger::info("    -Visuals from {}"sv, data.visualOwner);
					logger::info("      >Biped Models: M: '{}' F: '{}'"sv, obj->bipedModels[0].model.c_str(), obj->bipedModels[1].model.c_str());
					logger::info("      >1st Person Biped Models: M: '{}' F: '{}'"sv, obj->bipedModel1stPersons[0].model.c_str(), obj->bipedModel1stPersons[1].model.c_str());
					logger::info("      >Skin Textures: M: {:0X} F: {:0X}"sv, obj->skinTextures[0] ? obj->skinTextures[0]->formID : 0, obj->skinTextures[1] ? obj->skinTextures[1]->formID : 0);
					logger::info("      >Skin Texture Swap Lists: M: {:0X} F: {:0X}"sv, obj->skinTextureSwapLists[0] ? obj->skinTextureSwapLists[0]->formID : 0, obj->skinTextureSwapLists[1] ? obj->skinTextureSwapLists[1]->formID : 0);
				}
				if (patchedAudio) {
					logger::info("    -Audio from {}"sv, data.audioOwner);
					logger::info("      >Footstep Set: {:0X}"sv, obj->footstepSet ? obj->footstepSet->formID : 0);
				}
			}
		}
		readData = std::move(filteredData);
		ArmorAddon::_loadedAll = true;
		logger::info("Finished; Applied {} patches."sv, readData.size());
	}
}