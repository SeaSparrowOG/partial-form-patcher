#include "ArmorAddonPatcher.h"

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
		result &= ArmorAddon::Listen();
		return result;
	}

	void PerformDataLoadedOp()
	{
		auto* manager = ArmorAddonCache::GetSingleton();
		if (manager) {
			manager->OnDataLoaded();
		}
	}

	bool ArmorAddon::Listen() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESObjectARMA::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, Load);
		return true;
	}

	inline bool ArmorAddon::Load(RE::TESObjectARMA* a_this, RE::TESFile* a_file) {
		bool response = _load(a_this, a_file);
		auto* manager = ArmorAddonCache::GetSingleton();
		if (response && manager) {
			manager->OnAddonLoaded(a_this, a_file);
		}
		return response;
	}

	void ArmorAddonCache::OnAddonLoaded(RE::TESObjectARMA* a_addon,
		RE::TESFile* a_file)
	{
		// Preserve: Model, Footstep sounds.
		auto addonID = a_addon->formID;

		if (!mappedData.contains(addonID)) {
			auto newData = CachedData();
			newData.baseFootstepSound = a_addon->footstepSet;
			newData.baseModel_f = a_addon->bipedModels[1].model.c_str();
			newData.baseModel_m = a_addon->bipedModels[0].model.c_str();
			mappedData.emplace(addonID, std::move(newData));
			return;
		}

		auto& pair = mappedData.at(addonID);
		std::string newFModel = a_addon->bipedModels[1].model.c_str();
		std::string newMModel = a_addon->bipedModels[0].model.c_str();
		bool isModelMaster = false;
		bool isAudioMaster = false;

		pair.requiresProcessing |= pair.modelOwningFile || pair.soundOwningFile;
		bool replaced = false;

		if (isModelMaster) {
			pair.altModel_m = newMModel;
			pair.altModel_f = newFModel;
			replaced = true;
		}
		else {
			if (newFModel != pair.baseModel_f) {
				pair.altModel_f = newFModel;
				replaced = true;
			}
			if (newMModel != pair.baseModel_m) {
				pair.altModel_m = newMModel;
				replaced = true;
			}
			if (replaced) {
				pair.modelOwningFile = a_file;
			}
		}

		if (replaced) {
			pair.bipedObj = a_addon->bipedModelData;
			pair.bipedModels_0 = &a_addon->bipedModels[0];
			pair.bipedModels_1 = &a_addon->bipedModels[1];
			pair.bodyTextureSet_0 = a_addon->skinTextures[0];
			pair.bodyTextureSet_1 = a_addon->skinTextures[1];
			pair.skinTextureSwapLists_0 = a_addon->skinTextureSwapLists[0];
			pair.skinTextureSwapLists_1 = a_addon->skinTextureSwapLists[0];
		}

		if (isAudioMaster || a_addon->footstepSet != pair.baseFootstepSound) {
			pair.altFootstepSound = a_addon->footstepSet;
			pair.soundOwningFile = isAudioMaster ? pair.soundOwningFile : a_file;
		}
	}

	void ArmorAddonCache::OnDataLoaded()
	{
		logger::info("Found {} Armor Addons in files, processing...", mappedData.size());
		std::unordered_map<RE::FormID, CachedData> modified{};
		for (auto& [id, data] : mappedData) {
			auto* addon = RE::TESForm::LookupByID<RE::TESObjectARMA>(id);
			if (!addon || !data.requiresProcessing) {
				continue;
			}

			addon->bipedModels[0] = *data.bipedModels_0;
			addon->bipedModels[1] = *data.bipedModels_1;
			addon->bipedModelData = data.bipedObj;
			addon->skinTextures[0] = data.bodyTextureSet_0;
			addon->skinTextures[1] = data.bodyTextureSet_1;
			addon->skinTextureSwapLists[0] = data.skinTextureSwapLists_0;
			addon->skinTextureSwapLists[1] = data.skinTextureSwapLists_1;
			addon->bipedModels[1].model = data.altModel_f;
			addon->bipedModels[0].model = data.altModel_m;
			addon->footstepSet = data.altFootstepSound;
			modified.emplace(id, std::move(data));
		}
		mappedData = modified;
		logger::info("  >Finished processing {} armor addons."sv, mappedData.size());
	}
}