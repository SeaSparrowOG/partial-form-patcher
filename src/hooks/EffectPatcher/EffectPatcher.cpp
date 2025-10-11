#include "EffectPatcher.h"

#include "common/Utilities.h"
#include "Settings/INI/INISettings.h"

namespace {
	struct SoundPair
	{
		RE::MagicSystem::SoundID id{ 0 };
		RE::FormID sound{ 0 };
	};

    struct EffectSettingData
    {
    public:
        RE::EffectSetting::EffectSettingData::Flag flags{};
        float baseCost{ 0.0f };
        RE::FormID associatedForm{ 0 };
        RE::ActorValue associatedSkill{ RE::ActorValue::kNone };
        RE::ActorValue resistVariable{ RE::ActorValue::kNone };
        std::int16_t numCounterEffects{ 0 };
        RE::FormID light{ 0 };
        float taperWeight{ 0.0f };
        RE::FormID effectShader{ 0 };
        RE::FormID enchantShader{ 0 };
        std::int32_t minimumSkill{ 0 };
        std::int32_t spellmakingArea{ 0 };
        float spellmakingChargeTime{ 0.0f };
        float taperCurve{ 0.0f };
        float taperDuration{ 0.0f };
        float secondAVWeight{ 0.0f };
        RE::EffectSetting::Archetype archetype{ RE::EffectSetting::Archetype::kAbsorb };
        RE::ActorValue primaryAV;
        RE::FormID projectileBase{ 0 };
        RE::FormID explosion{ 0 };
        RE::MagicSystem::CastingType castingType{ RE::MagicSystem::CastingType::kFireAndForget };
        RE::MagicSystem::Delivery delivery{ RE::MagicSystem::Delivery::kAimed };
        RE::ActorValue secondaryAV{ 0 };
        RE::FormID castingArt{ 0 };
        RE::FormID hitEffectArt{ 0 };
        RE::FormID impactDataSet{ 0 };
        float skillUsageMult{ 0.0f };
        RE::FormID dualCastData{ 0 };
        float dualCastScale{ 0.0f };
        RE::FormID enchantEffectArt{ 0 };
        RE::FormID hitVisuals{ 0 };
        RE::FormID enchantVisuals{ 0 };
        RE::FormID equipAbility{ 0 };
        RE::FormID imageSpaceMod{ 0 };
        RE::FormID perk{ 0 };
        RE::SOUND_LEVEL castingSoundLevel{ RE::SOUND_LEVEL::kQuiet };
        float aiScore{ 0.0f };
        float aiDelayTimer{ 0.0f };
    };
}

namespace Hooks::EffectPatcher
{
	bool ListenForEffects() {
		logger::info("  Installing Effect Patcher..."sv);
		bool result = true;
		auto shouldInstall = Settings::INI::GetSetting<bool>(Settings::INI::GENERAL_MGEF).value_or(false);
		if (!shouldInstall) {
			logger::info("    >User opted to not install the patcher."sv);
			return result;
		}
		result &= Effect::HookEffectSetting();
		return result;
	}

	bool Effect::HookEffectSetting() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::EffectSetting::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, LoadEffectSetting);
		return true;
	}

	bool Effect::LoadEffectSetting(RE::EffectSetting* a_this, RE::TESFile* a_file) {
		auto* manager = EffectCache::GetSingleton();
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
			bool found = false;
			auto formID = a_this->formID;
			while (!found && duplicate->SeekNextForm(true)) {
				if (duplicate->currentform.formID != formID) {
					continue;
				}
				found = true;
			}
			if (!found) {
				//duplicate->CloseTES(true);
				return result;
			}
			manager->OnEffectSettingLoaded(a_this, duplicate);
			//duplicate->CloseTES(true);
		}
		return result;
	}

	void PerformDataLoadedOp() {
		auto* manager = EffectCache::GetSingleton();
		if (manager) {
			manager->PatchEffectSettings();
		}
	}

    void EffectCache::OnEffectSettingLoaded(RE::EffectSetting* a_obj,
        RE::TESFile* a_file)
    {
		auto formID = a_obj->formID;
		RE::FormID fileLight = 0;
		RE::FormID fileEffectShader = 0;
		RE::FormID fileEnchantShader = 0;
		RE::FormID fileCastingArt = 0;
		RE::FormID fileHitEffectArt = 0;
		RE::FormID fileImpactDataSet = 0;
		RE::FormID fileEnchantEffectArt = 0;
		RE::FormID fileHitVisuals = 0;
		RE::FormID fileEnchantVisuals = 0;
		RE::FormID fileImageSpaceMod = 0;

		RE::FormID fileChargingSound = 0;
		RE::FormID fileReadyLoopSound = 0;
		RE::FormID fileReleaseSound = 0;
		RE::FormID fileCastLoopSound = 0;

		while (a_file->SeekNextSubrecord()) {
			// Data
			if (Utilities::IsSubrecord(a_file, "DATA")) {
				EffectSettingData rawData = EffectSettingData();
				if (a_file->ReadData(&rawData, a_file->actualChunkSize)) {
					fileLight = Utilities::GetAbsoluteFormID(rawData.light, a_file);
					fileEffectShader = Utilities::GetAbsoluteFormID(rawData.effectShader, a_file);
					fileEnchantShader = Utilities::GetAbsoluteFormID(rawData.enchantShader, a_file);
					fileCastingArt = Utilities::GetAbsoluteFormID(rawData.castingArt, a_file);
					fileHitEffectArt = Utilities::GetAbsoluteFormID(rawData.hitEffectArt, a_file);
					fileImpactDataSet = Utilities::GetAbsoluteFormID(rawData.impactDataSet, a_file);
					fileEnchantEffectArt = Utilities::GetAbsoluteFormID(rawData.enchantEffectArt, a_file);
					fileHitVisuals = Utilities::GetAbsoluteFormID(rawData.hitVisuals, a_file);
					fileEnchantVisuals = Utilities::GetAbsoluteFormID(rawData.enchantVisuals, a_file);
					fileImageSpaceMod = Utilities::GetAbsoluteFormID(rawData.imageSpaceMod, a_file);
				}
			}
			// Sounds
			else if (Utilities::IsSubrecord(a_file, "SNDD")) {
				std::vector<SoundPair> tempSounds;
				std::size_t count = a_file->actualChunkSize / sizeof(SoundPair);
				tempSounds.resize(count);
				if (a_file->ReadData(tempSounds.data(), a_file->actualChunkSize)) {
					for (auto& pair : tempSounds) {
						switch (pair.id) {
						case RE::MagicSystem::SoundID::kCharge:
							fileChargingSound = Utilities::GetAbsoluteFormID(pair.sound, a_file);
							break;
						case RE::MagicSystem::SoundID::kReadyLoop:
							fileReadyLoopSound = Utilities::GetAbsoluteFormID(pair.sound, a_file);
							break;
						case RE::MagicSystem::SoundID::kRelease:
							fileReleaseSound = Utilities::GetAbsoluteFormID(pair.sound, a_file);
							break;
						case RE::MagicSystem::SoundID::kCastLoop:
							fileCastLoopSound = Utilities::GetAbsoluteFormID(pair.sound, a_file);
							break;
						default:
							break;
						}
					}
				}
			}
		}

		if (!readData.contains(formID)) {
			auto newData = ReadData();
			newData.baseLight = fileLight;
			newData.baseEffectShader = fileEffectShader;
			newData.baseEnchantShader = fileEnchantShader;
			newData.baseCastingArt = fileCastingArt;
			newData.baseHitEffectArt = fileHitEffectArt;
			newData.baseImpactDataSet = fileImpactDataSet;
			newData.baseEnchantEffectArt = fileEnchantEffectArt;
			newData.baseHitVisuals = fileHitVisuals;
			newData.baseEnchantVisuals = fileEnchantVisuals;
			newData.baseImageSpaceMod = fileImageSpaceMod;
			newData.baseChargingSound = fileChargingSound;
			newData.baseReadyLoopSound = fileReadyLoopSound;
			newData.baseReleaseSound = fileReleaseSound;
			newData.baseCastLoopSound = fileCastLoopSound;
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

		bool overwritesBaseTextures = fileLight != data.baseLight;
		overwritesBaseTextures |= fileEffectShader != data.baseEffectShader;
		overwritesBaseTextures |= fileEnchantShader != data.baseEnchantShader;
		overwritesBaseTextures |= fileCastingArt != data.baseCastingArt;
		overwritesBaseTextures |= fileHitEffectArt != data.baseHitEffectArt;
		overwritesBaseTextures |= fileImpactDataSet != data.baseImpactDataSet;
		overwritesBaseTextures |= fileEnchantEffectArt != data.baseEnchantEffectArt;
		overwritesBaseTextures |= fileHitVisuals != data.baseHitVisuals;
		overwritesBaseTextures |= fileEnchantVisuals != data.baseEnchantVisuals;
		overwritesBaseTextures |= fileImageSpaceMod != data.baseImageSpaceMod;

		bool overwritesBaseAudio = fileChargingSound != data.baseChargingSound;
		overwritesBaseAudio |= fileReadyLoopSound != data.baseReadyLoopSound;
		overwritesBaseAudio |= fileReleaseSound != data.baseReleaseSound;
		overwritesBaseAudio |= fileCastLoopSound != data.baseCastLoopSound;

		if (isOverwritingMasterVisuals || overwritesBaseTextures)
		{
			data.altCastingArt = fileCastingArt;
			data.altEffectShader = fileEffectShader;
			data.altEnchantShader = fileEnchantShader;
			data.altEnchantEffectArt = fileEnchantEffectArt;
			data.altHitEffectArt = fileHitEffectArt;
			data.altHitVisuals = fileHitVisuals;
			data.altImageSpaceMod = fileImageSpaceMod;
			data.altImpactDataSet = fileImpactDataSet;
			data.altLight = fileLight;
			data.altEnchantVisuals = fileEnchantVisuals;
			data.visualOwner = a_file->GetFilename();

			data.holdsData = true;
		}
		if (isOverwritingMasterAudio || overwritesBaseAudio)
		{
			data.altChargingSound = fileChargingSound;
			data.altReadyLoopSound = fileReadyLoopSound;
			data.altReleaseSound = fileReleaseSound;
			data.altCastLoopSound = fileCastLoopSound;
			data.audioOwner = a_file->GetFilename();
			data.holdsData = true;
		}
    }

	void EffectCache::ReapplyChanges(RE::EffectSetting* a_obj)
	{
		auto formID = a_obj->formID;
		if (!readData.contains(formID)) {
			return;
		}
		auto& data = readData.at(formID);
		if (!data.overwritten) {
			return;
		}

		if (!data.audioOwner.empty()) {
			auto* charge = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altChargingSound);
			auto* ready = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altReadyLoopSound);
			auto* release = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altReleaseSound);
			auto* cast = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altCastLoopSound);
			RE::BGSSoundDescriptorForm* currCharge = nullptr;
			RE::BGSSoundDescriptorForm* currReady = nullptr;
			RE::BGSSoundDescriptorForm* currRelease = nullptr;
			RE::BGSSoundDescriptorForm* currCast = nullptr;
			for (auto& soundPair : a_obj->effectSounds) {
				switch (soundPair.id) {
				case RE::MagicSystem::SoundID::kCharge:
					currCharge = soundPair.sound;
					break;
				case RE::MagicSystem::SoundID::kReadyLoop:
					currReady = soundPair.sound;
					break;
				case RE::MagicSystem::SoundID::kRelease:
					currRelease = soundPair.sound;
					break;
				case RE::MagicSystem::SoundID::kCastLoop:
					currCast = soundPair.sound;
					break;
				default:
					break;
				}
			}
			bool needsPatch = false;
			needsPatch |= (charge != currCharge) && charge;
			needsPatch |= (ready != currReady) && ready;
			needsPatch |= (release != currRelease) && release;
			needsPatch |= (cast != currCast) && cast;
			if (needsPatch) {
				auto& arr = a_obj->effectSounds;
				arr.clear();
				if (charge) {
					auto newPair = RE::EffectSetting::SoundPair();
					newPair.id = RE::MagicSystem::SoundID::kCharge;
					newPair.sound = charge;
					arr.push_back(newPair);
				}
				if (ready) {
					auto newPair = RE::EffectSetting::SoundPair();
					newPair.id = RE::MagicSystem::SoundID::kReadyLoop;
					newPair.sound = ready;
					arr.push_back(newPair);
				}
				if (release) {
					auto newPair = RE::EffectSetting::SoundPair();
					newPair.id = RE::MagicSystem::SoundID::kRelease;
					newPair.sound = release;
					arr.push_back(newPair);
				}
				if (cast) {
					auto newPair = RE::EffectSetting::SoundPair();
					newPair.id = RE::MagicSystem::SoundID::kCastLoop;
					newPair.sound = cast;
					arr.push_back(newPair);
				}
			}
		}

		if (!data.visualOwner.empty()) {
			bool needsPatch = false;
			auto* light = RE::TESForm::LookupByID<RE::TESObjectLIGH>(data.altLight);
			auto* effectShader = RE::TESForm::LookupByID<RE::TESEffectShader>(data.altEffectShader);
			auto* enchantShader = RE::TESForm::LookupByID<RE::TESEffectShader>(data.altEnchantShader);
			auto* castingArt = RE::TESForm::LookupByID<RE::BGSArtObject>(data.altCastingArt);
			auto* hitEffectArt = RE::TESForm::LookupByID<RE::BGSArtObject>(data.altHitEffectArt);
			auto* impactDataSet = RE::TESForm::LookupByID<RE::BGSImpactDataSet>(data.altImpactDataSet);
			auto* enchantEffectArt = RE::TESForm::LookupByID<RE::BGSArtObject>(data.altEnchantEffectArt);
			auto* hitVisuals = RE::TESForm::LookupByID<RE::BGSReferenceEffect>(data.altHitVisuals);
			auto* enchantVisuals = RE::TESForm::LookupByID<RE::BGSReferenceEffect>(data.altEnchantVisuals);
			auto* imageSpaceMod = RE::TESForm::LookupByID<RE::TESImageSpaceModifier>(data.altImageSpaceMod);
			auto& effectData = a_obj->data;
			needsPatch |= (light != effectData.light) && light;
			needsPatch |= (effectShader != effectData.effectShader) && effectShader;
			needsPatch |= (enchantShader != effectData.enchantShader) && enchantShader;
			needsPatch |= (castingArt != effectData.castingArt) && castingArt;
			needsPatch |= (hitEffectArt != effectData.hitEffectArt) && hitEffectArt;
			needsPatch |= (impactDataSet != effectData.impactDataSet) && impactDataSet;
			needsPatch |= (enchantEffectArt != effectData.enchantEffectArt) && enchantEffectArt;
			needsPatch |= (hitVisuals != effectData.hitVisuals) && hitVisuals;
			needsPatch |= (enchantVisuals != effectData.enchantVisuals) && enchantVisuals;
			needsPatch |= (imageSpaceMod != effectData.imageSpaceMod) && imageSpaceMod;
			if (needsPatch) {
				effectData.light = light ? light : effectData.light;
				effectData.effectShader = effectShader ? effectShader : effectData.effectShader;
				effectData.enchantShader = enchantShader ? enchantShader : effectData.enchantShader;
				effectData.castingArt = castingArt ? castingArt : effectData.castingArt;
				effectData.hitEffectArt = hitEffectArt ? hitEffectArt : effectData.hitEffectArt;
				effectData.impactDataSet = impactDataSet ? impactDataSet : effectData.impactDataSet;
				effectData.enchantEffectArt = enchantEffectArt ? enchantEffectArt : effectData.enchantEffectArt;
				effectData.hitVisuals = hitVisuals ? hitVisuals : effectData.hitVisuals;
				effectData.enchantVisuals = enchantVisuals ? enchantVisuals : effectData.enchantVisuals;
				effectData.imageSpaceMod = imageSpaceMod ? imageSpaceMod : effectData.imageSpaceMod;
			}
		}
	}

	void EffectCache::PatchEffectSettings()
	{
		logger::info("Patching {} Effects..."sv, readData.size());

		std::unordered_map<RE::FormID, ReadData> filteredData;
		for (auto& [id, data] : readData) {
			if (!data.overwritten) {
				continue;
			}
			auto* obj = RE::TESForm::LookupByID<RE::EffectSetting>(id);
			if (!obj) {
				logger::error("  >Deleted record at {:0X}."sv, id);
				continue;
			}

			bool patchedAudio = false;
			bool patchedVisuals = false;
			if (!data.audioOwner.empty()) {
				auto* charge = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altChargingSound);
				auto* ready = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altReadyLoopSound);
				auto* release = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altReleaseSound);
				auto* cast = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altCastLoopSound);

				RE::BGSSoundDescriptorForm* currCharge = nullptr;
				RE::BGSSoundDescriptorForm* currReady = nullptr;
				RE::BGSSoundDescriptorForm* currRelease = nullptr;
				RE::BGSSoundDescriptorForm* currCast = nullptr;

				for (auto& soundPair : obj->effectSounds) {
					switch (soundPair.id) {
						case RE::MagicSystem::SoundID::kCharge:
							currCharge = soundPair.sound;
							break;
						case RE::MagicSystem::SoundID::kReadyLoop:
							currReady = soundPair.sound;
							break;
						case RE::MagicSystem::SoundID::kRelease:
							currRelease = soundPair.sound;
							break;
						case RE::MagicSystem::SoundID::kCastLoop:
							currCast = soundPair.sound;
							break;
						default:
							break;
					}
				}

				bool needsPatch = false;
				needsPatch |= (charge != currCharge) && charge;
				needsPatch |= (ready != currReady) && ready;
				needsPatch |= (release != currRelease) && release;
				needsPatch |= (cast != currCast) && cast;

				if (needsPatch) {
					auto& arr = obj->effectSounds;
					arr.clear();
					if (charge) {
						auto newPair = RE::EffectSetting::SoundPair();
						newPair.id = RE::MagicSystem::SoundID::kCharge;
						newPair.sound = charge;

						arr.push_back(newPair);
					}
					if (ready) {
						auto newPair = RE::EffectSetting::SoundPair();
						newPair.id = RE::MagicSystem::SoundID::kReadyLoop;
						newPair.sound = ready;
						arr.push_back(newPair);
					}
					if (release) {
						auto newPair = RE::EffectSetting::SoundPair();
						newPair.id = RE::MagicSystem::SoundID::kRelease;
						newPair.sound = release;
						arr.push_back(newPair);
					}
					if (cast) {
						auto newPair = RE::EffectSetting::SoundPair();
						newPair.id = RE::MagicSystem::SoundID::kCastLoop;
						newPair.sound = cast;
						arr.push_back(newPair);
					}
				}
				patchedAudio |= needsPatch;
			}

			if (!data.visualOwner.empty()) {
				bool needsPatch = false;
				
				auto* light = RE::TESForm::LookupByID<RE::TESObjectLIGH>(data.altLight);
				auto* effectShader = RE::TESForm::LookupByID<RE::TESEffectShader>(data.altEffectShader);
				auto* enchantShader = RE::TESForm::LookupByID<RE::TESEffectShader>(data.altEnchantShader);
				auto* castingArt = RE::TESForm::LookupByID<RE::BGSArtObject>(data.altCastingArt);
				auto* hitEffectArt = RE::TESForm::LookupByID<RE::BGSArtObject>(data.altHitEffectArt);
				auto* impactDataSet = RE::TESForm::LookupByID<RE::BGSImpactDataSet>(data.altImpactDataSet);
				auto* enchantEffectArt = RE::TESForm::LookupByID<RE::BGSArtObject>(data.altEnchantEffectArt);
				auto* hitVisuals = RE::TESForm::LookupByID<RE::BGSReferenceEffect>(data.altHitVisuals);
				auto* enchantVisuals = RE::TESForm::LookupByID<RE::BGSReferenceEffect>(data.altEnchantVisuals);
				auto* imageSpaceMod = RE::TESForm::LookupByID<RE::TESImageSpaceModifier>(data.altImageSpaceMod);

				auto& effectData = obj->data;
				needsPatch |= (light != effectData.light) && light;
				needsPatch |= (effectShader != effectData.effectShader) && effectShader;
				needsPatch |= (enchantShader != effectData.enchantShader) && enchantShader;
				needsPatch |= (castingArt != effectData.castingArt) && castingArt;
				needsPatch |= (hitEffectArt != effectData.hitEffectArt) && hitEffectArt;
				needsPatch |= (impactDataSet != effectData.impactDataSet) && impactDataSet;
				needsPatch |= (enchantEffectArt != effectData.enchantEffectArt) && enchantEffectArt;
				needsPatch |= (hitVisuals != effectData.hitVisuals) && hitVisuals;
				needsPatch |= (enchantVisuals != effectData.enchantVisuals) && enchantVisuals;
				needsPatch |= (imageSpaceMod != effectData.imageSpaceMod) && imageSpaceMod;

				if (needsPatch) {
					effectData.light = light;
					effectData.effectShader = effectShader;
					effectData.enchantShader = enchantShader;
					effectData.castingArt = castingArt;
					effectData.hitEffectArt = hitEffectArt;
					effectData.impactDataSet = impactDataSet;
					effectData.enchantEffectArt = enchantEffectArt;
					effectData.hitVisuals = hitVisuals;
					effectData.enchantVisuals = enchantVisuals;
					effectData.imageSpaceMod = imageSpaceMod;
				}
			}

			if (patchedAudio || patchedVisuals) {
				filteredData.emplace(id, data);
				logger::info("  >Patched Effect {} at {:0X}. Changes:"sv, obj->GetName(), id);
				if (patchedVisuals) {
					logger::info("    -Visuals from {}"sv, data.visualOwner);
					logger::info("      >Light: {:0X}"sv, obj->data.light ? obj->data.light->formID : 0);
					logger::info("      >Effect Shader: {:0X}"sv, obj->data.effectShader ? obj->data.effectShader->formID : 0);
					logger::info("      >Enchant Shader: {:0X}"sv, obj->data.enchantShader ? obj->data.enchantShader->formID : 0);
					logger::info("      >Casting Art: {:0X}"sv, obj->data.castingArt ? obj->data.castingArt->formID : 0);
					logger::info("      >Hit Effect Art: {:0X}"sv, obj->data.hitEffectArt ? obj->data.hitEffectArt->formID : 0);
					logger::info("      >Impact Data Set: {:0X}"sv, obj->data.impactDataSet ? obj->data.impactDataSet->formID : 0);
					logger::info("      >Enchant Effect Art: {:0X}"sv, obj->data.enchantEffectArt ? obj->data.enchantEffectArt->formID : 0);
					logger::info("      >Hit Visuals: {:0X}"sv, obj->data.hitVisuals ? obj->data.hitVisuals->formID : 0);
					logger::info("      >Enchant Visuals: {:0X}"sv, obj->data.enchantVisuals ? obj->data.enchantVisuals->formID : 0);
					logger::info("      >Image Space Mod: {:0X}"sv, obj->data.imageSpaceMod ? obj->data.imageSpaceMod->formID : 0);
				}
				if (patchedAudio) {
					logger::info("    -Audio from {}"sv, data.audioOwner);
					for (auto& soundPair : obj->effectSounds) {
						switch (soundPair.id) {
						case RE::MagicSystem::SoundID::kCharge:
							logger::info("      >Charging Sound: {:0X}"sv, soundPair.sound ? soundPair.sound->formID : 0);
							break;
						case RE::MagicSystem::SoundID::kReadyLoop:
							logger::info("      >Ready Loop Sound: {:0X}"sv, soundPair.sound ? soundPair.sound->formID : 0);
							break;
						case RE::MagicSystem::SoundID::kRelease:
							logger::info("      >Release Sound: {:0X}"sv, soundPair.sound ? soundPair.sound->formID : 0);
							break;
						case RE::MagicSystem::SoundID::kCastLoop:
							logger::info("      >Cast Loop Sound: {:0X}"sv, soundPair.sound ? soundPair.sound->formID : 0);
							break;
						default:
							break;
						}
					}
				}
			}
		}
		readData = std::move(filteredData);
		Effect::_loadedAll = true;
		logger::info("Finished; Applied {} patches."sv, readData.size());
	}
}