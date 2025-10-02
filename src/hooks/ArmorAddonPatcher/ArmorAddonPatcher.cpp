#include "ArmorAddonPatcher.h"

#include "Settings/INI/INISettings.h"

namespace Hooks::ArmorAddonPatcher
{
	std::string GetEditorID(const RE::TESForm* a_form)
	{
		using _GetFormEditorID = const char* (*)(std::uint32_t);
		switch (a_form->GetFormType()) {
		case RE::FormType::Keyword:
		case RE::FormType::LocationRefType:
		case RE::FormType::Action:
		case RE::FormType::MenuIcon:
		case RE::FormType::Global:
		case RE::FormType::HeadPart:
		case RE::FormType::Race:
		case RE::FormType::Sound:
		case RE::FormType::Script:
		case RE::FormType::Navigation:
		case RE::FormType::Cell:
		case RE::FormType::WorldSpace:
		case RE::FormType::Land:
		case RE::FormType::NavMesh:
		case RE::FormType::Dialogue:
		case RE::FormType::Quest:
		case RE::FormType::Idle:
		case RE::FormType::AnimatedObject:
		case RE::FormType::ImageAdapter:
		case RE::FormType::VoiceType:
		case RE::FormType::Ragdoll:
		case RE::FormType::DefaultObject:
		case RE::FormType::MusicType:
		case RE::FormType::StoryManagerBranchNode:
		case RE::FormType::StoryManagerQuestNode:
		case RE::FormType::StoryManagerEventNode:
		case RE::FormType::SoundRecord:
			return a_form->GetFormEditorID();
		default:
		{
			static auto tweaks = REX::W32::GetModuleHandleW(L"po3_Tweaks");
			static auto func = reinterpret_cast<_GetFormEditorID>(REX::W32::GetProcAddress(tweaks, "GetFormEditorID"));
			if (func) {
				return func(a_form->formID);
			}
			return {};
		}
		}
	}

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
			newData.associatedRaceArray = a_addon->additionalRaces;
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

		pair.requiresProcessing |= pair.modelOwningFile || pair.modelOwningFile;

		if (isModelMaster || (newFModel != pair.baseModel_f || newMModel != pair.baseModel_m)) 
		{
			pair.altModel_m = newMModel;
			pair.altModel_f = newFModel;
			pair.associatedRaceArray = a_addon->additionalRaces;
			pair.modelOwningFile = isModelMaster ? pair.modelOwningFile : a_file;
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

#ifndef NDEBUG
			auto name = GetEditorID(addon);
			LOG_DEBUG("    >{}"sv, name);
#endif

			addon->bipedModels[1].model = data.altModel_f;
			addon->bipedModels[0].model = data.altModel_m;
			modified.emplace(id, std::move(data));
		}
		mappedData = modified;
		logger::info("  >Finished processing {} armor addons."sv, mappedData.size());
	}
}