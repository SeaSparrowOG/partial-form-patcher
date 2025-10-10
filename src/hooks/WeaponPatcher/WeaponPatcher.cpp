#include "WeaponPatcher.h"

#include "common/Utilities.h"
#include "Settings/INI/INISettings.h"

namespace Hooks::WeaponPatcher
{
	bool ListenForWeapons() {
		logger::info("  Installing Weapon Patcher..."sv);
		bool result = true;
		auto shouldInstall = Settings::INI::GetSetting<bool>(Settings::INI::GENERAL_WEAP).value_or(false);
		if (!shouldInstall) {
			logger::info("    >User opted to not install the patcher."sv);
			return result;
		}
		result &= Weapon::HookTESObjectWEAP();
		return result;
	}

	bool Weapon::HookTESObjectWEAP() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESObjectWEAP::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, LoadWeaponFromFile);
		return true;
	}

	inline bool Weapon::LoadWeaponFromFile(RE::TESObjectWEAP* a_this, 
		RE::TESFile* a_file)
	{
		auto* duplicate = a_file ? a_file->Duplicate() : nullptr;
		bool result = _load(a_this, a_file);
		if (result && a_this && duplicate) {
			auto* manager = WeaponCache::GetSingleton();
			if (manager) {
				if (!duplicate->Seek(0)) {
					logger::error("Failed to seek 0 for weapon at {:0X}."sv, a_this->formID);
					return result;
				}
				bool found = false;
				auto formID = a_this->formID;
				while (!found && duplicate->SeekNextForm(true)) {
					if (duplicate->currentform.formID != formID) {
						continue;
					}
					found = true;
				}
				manager->OnWeaponLoaded(a_this, duplicate);
			}
		}
		return result;
	}

	void PerformDataLoadedOp() {
		auto* manager = WeaponCache::GetSingleton();
		if (manager) {
			manager->OnDataLoaded();
		}
	}

	void WeaponCache::OnWeaponLoaded(RE::TESObjectWEAP* a_obj,
		RE::TESFile* a_file)
	{
		auto formID = a_obj->formID;
		std::string_view fileName = a_file->fileName;
		RE::BSFixedString fileModel = a_obj->model;
		RE::FormID fileFirstPersonModel = 0;
		RE::FormID filePickupSound = 0;
		RE::FormID filePutdownSound = 0;
		RE::FormID fileImpactDataSet = 0;
		RE::FormID fileBlockBashImpactDataSet = 0;
		RE::FormID fileBlockAlternateMaterial = 0;
		RE::FormID fileAttackSound = 0;
		RE::FormID fileAttackLoop = 0;
		RE::FormID fileAttackFail = 0;
		RE::FormID fileIdle = 0;
		RE::FormID fileEquip= 0;
		RE::FormID fileUnEquip = 0;

		while (a_file->SeekNextSubrecord()) {
			// Pickup
			if (Utilities::IsSubrecord(a_file, "YNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					filePickupSound = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// Put Down
			else if (Utilities::IsSubrecord(a_file, "ZNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					filePutdownSound = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// Block Impact Data Set
			else if (Utilities::IsSubrecord(a_file, "BIDS")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileBlockBashImpactDataSet = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// Block Alternate Material
			else if (Utilities::IsSubrecord(a_file, "BAMT")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileBlockAlternateMaterial = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// Impact Data Set
			else if (Utilities::IsSubrecord(a_file, "INAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileImpactDataSet = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// Attack Sound
			else if (Utilities::IsSubrecord(a_file, "SNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileAttackSound = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// Attack Loop Sound 
			else if (Utilities::IsSubrecord(a_file, "7NAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileAttackLoop = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// Attack Fail
			else if (Utilities::IsSubrecord(a_file, "TNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileAttackFail = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// Idle
			else if (Utilities::IsSubrecord(a_file, "UNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileIdle = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// Attack Fail
			else if (Utilities::IsSubrecord(a_file, "NAM9")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileEquip = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// Attack Fail
			else if (Utilities::IsSubrecord(a_file, "NAM8")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileUnEquip = Utilities::GetAbsoluteFormID(retrieved, a_file);
				}
			}
			// First Person Model
			else if (Utilities::IsSubrecord(a_file, "WNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileFirstPersonModel = Utilities::GetAbsoluteFormID(retrieved, a_file);
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
			newData.baseAttackFailSound = fileAttackFail;
			newData.baseAttackLoopSound = fileAttackLoop;
			newData.baseAttackSound = fileAttackSound;
			newData.baseBlockAlternateMaterial = fileBlockAlternateMaterial;
			newData.baseBlockBashImpactDataSet = fileBlockBashImpactDataSet;
			newData.baseEquipSound = fileEquip;
			newData.baseFirstPersonMesh = fileFirstPersonModel;
			newData.baseIdleSound = fileIdle;
			newData.baseImpactDataSet = fileImpactDataSet;
			newData.baseModel = fileModel;
			newData.basePickUpSound = filePickupSound;
			newData.basePutDownSound = filePutdownSound;
			newData.baseUnEquipSound = fileUnEquip;
			readData.emplace(formID, std::move(newData));
			return;
		}

		auto& data = readData.at(formID);
		data.overwritten |= data.holdsData;

		auto& masters = a_file->masters;
		auto end = masters.end();
		bool isOverwritingMasterVisuals = false;
		bool isOverwritingMasterAudio = false;
		bool isOverwritingMasterImpact = false;
		for (auto it = masters.begin(); (!isOverwritingMasterVisuals || !isOverwritingMasterAudio || !isOverwritingMasterImpact) && it != end; ++it) {
			if (!*it) { // Should never happen, these are file names that REQUIRE a name.
				continue;
			}
			auto name = std::string((*it));
			isOverwritingMasterVisuals |= name == data.visualOwner;
			isOverwritingMasterAudio |= name == data.audioOwner;
			isOverwritingMasterImpact |= name == data.impactOwner;
		}

		bool overwritesBaseTextures = fileModel != data.baseModel;

		bool overwritesBaseAudio = filePickupSound != data.basePickUpSound;
		overwritesBaseAudio |= filePutdownSound != data.basePutDownSound;
		overwritesBaseAudio |= fileAttackFail != data.altAttackFailSound;
		overwritesBaseAudio |= fileAttackLoop != data.altAttackLoopSound;
		overwritesBaseAudio |= fileAttackSound != data.altAttackSound;
		overwritesBaseAudio |= fileIdle != data.altIdleSound;
		overwritesBaseAudio |= fileEquip != data.altEquipSound;
		overwritesBaseAudio |= fileUnEquip != data.altUnEquipSound;

		bool overwritesBaseImpact = fileImpactDataSet != data.baseImpactDataSet;
		overwritesBaseImpact |= fileBlockBashImpactDataSet != data.baseBlockBashImpactDataSet;
		overwritesBaseImpact |= fileBlockAlternateMaterial != data.baseBlockAlternateMaterial;

		if (isOverwritingMasterImpact || overwritesBaseImpact)
		{
			data.altImpactDataSet = fileImpactDataSet;
			data.altBlockBashImpactDataSet = fileBlockBashImpactDataSet;
			data.altBlockAlternateMaterial = fileBlockAlternateMaterial;
			data.impactOwner = fileName;
			data.holdsData = true;
		}

		if (isOverwritingMasterVisuals || overwritesBaseTextures)
		{
			data.altModel = fileModel;
			data.altFirstPersonMesh = fileFirstPersonModel;
			data.visualOwner = fileName;
			data.holdsData = true;
		}
		if (isOverwritingMasterAudio || overwritesBaseAudio)
		{
			data.altPickUpSound = filePickupSound;
			data.altPutDownSound = filePutdownSound;
			data.altAttackSound = fileAttackSound;
			data.altAttackLoopSound = fileAttackLoop;
			data.altAttackFailSound = fileAttackFail;
			data.altIdleSound = fileIdle;
			data.altEquipSound = fileEquip;
			data.altUnEquipSound = fileUnEquip;
			data.audioOwner = fileName;
			data.holdsData = true;
		}
	}

	void WeaponCache::OnDataLoaded() {
		logger::info("Patching {} Weapons..."sv, readData.size());

		std::unordered_map<RE::FormID, ReadData> filteredData;
		for (auto& [id, data] : readData) {
			if (!data.overwritten) {
				continue;
			}
			auto* obj = RE::TESForm::LookupByID<RE::TESObjectWEAP>(id);
			if (!obj) {
				logger::error("  >Deleted record at {:0X}."sv, id);
				continue;
			}

			bool patchedAudio = false;
			bool patchedImpact = false;
			bool patchedVisuals = false;
			if (!data.audioOwner.empty()) {
				auto* putDown = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altPutDownSound);
				auto* pickUp = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altPickUpSound);
				auto* attack = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altAttackSound);
				auto* attackLoop = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altAttackLoopSound);
				auto* attackFail = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altAttackFailSound);
				auto* idle = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altIdleSound);
				auto* equip = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altEquipSound);
				auto* unEquip = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(data.altUnEquipSound);

				bool needsPatch = false;
				needsPatch |= (putDown != obj->putdownSound) && putDown;
				needsPatch |= (pickUp != obj->pickupSound) && pickUp;
				needsPatch |= (attack != obj->attackSound) && attack;
				needsPatch |= (attackLoop != obj->attackLoopSound) && attackLoop;
				needsPatch |= (attackFail != obj->attackFailSound) && attackFail;
				needsPatch |= (idle != obj->idleSound) && idle;
				needsPatch |= (equip != obj->equipSound) && equip;
				needsPatch |= (unEquip != obj->unequipSound) && unEquip;

				if (needsPatch) {
					obj->pickupSound = pickUp;
					obj->putdownSound = putDown;
					obj->attackSound = attack;
					obj->attackLoopSound = attackLoop;
					obj->attackFailSound = attackFail;
					obj->idleSound = idle;
					obj->equipSound = equip;
					obj->unequipSound = unEquip;
				}
				patchedAudio |= needsPatch;
			}
			if (!data.impactOwner.empty()) {
				auto* impactDataSet = RE::TESForm::LookupByID<RE::BGSImpactDataSet>(data.altImpactDataSet);
				auto* blockBashDataSet = RE::TESForm::LookupByID<RE::BGSImpactDataSet>(data.altBlockBashImpactDataSet);
				auto* blockMaterial = RE::TESForm::LookupByID<RE::BGSMaterialType>(data.altBlockAlternateMaterial);
			
				bool needsPatch = false;
				needsPatch |= (impactDataSet != obj->impactDataSet) && impactDataSet;
				needsPatch |= (blockBashDataSet != obj->blockBashImpactDataSet) && blockBashDataSet;
				needsPatch |= (blockMaterial != obj->altBlockMaterialType) && blockMaterial;
				if (needsPatch) {
					obj->impactDataSet = impactDataSet;
					obj->blockBashImpactDataSet = blockBashDataSet;
					obj->altBlockMaterialType = blockMaterial;
				}
				patchedImpact |= needsPatch;
			}
			if (!data.visualOwner.empty()) {
				auto* newFirstPersonModel = RE::TESForm::LookupByID<RE::TESObjectSTAT>(data.altFirstPersonMesh);
				bool needsPatch = false;
				needsPatch |= (data.altModel != obj->model) && !data.altModel.empty();
				needsPatch |= (newFirstPersonModel != obj->firstPersonModelObject) && newFirstPersonModel;
				patchedVisuals |= needsPatch;
				if (needsPatch) {
					obj->firstPersonModelObject = newFirstPersonModel ? newFirstPersonModel : obj->firstPersonModelObject;
					obj->SetModel(data.altModel.c_str());
				}
			}
			if (patchedAudio || patchedImpact || patchedVisuals) {
				filteredData.emplace(id, data);
				
				logger::info("  >Patched weapon {} at {:0X}. Changes:"sv, obj->GetName(), id);
				if (patchedVisuals) {
					logger::info("    -Visuals from {}"sv, data.visualOwner);
					logger::info("      >Model: {}"sv, obj->model.c_str());
					logger::info("      >First Person Model: {:0X}"sv, obj->firstPersonModelObject ? obj->firstPersonModelObject->formID : 0);
				}
				if (patchedImpact) {
					logger::info("    -Impact from {}"sv, data.impactOwner);
					logger::info("      >Impact Data Set: {:0X}"sv, obj->impactDataSet ? obj->impactDataSet->formID : 0);
					logger::info("      >Block Bash Data Set: {:0X}"sv, obj->blockBashImpactDataSet ? obj->blockBashImpactDataSet->formID : 0);
					logger::info("      >Block Alternate Material: {:0X}"sv, obj->altBlockMaterialType ? obj->altBlockMaterialType->formID : 0);
				}
				if (patchedAudio) {
					logger::info("    -Audio from {}"sv, data.audioOwner);
					logger::info("      >Pickup: {:0X}"sv, obj->pickupSound ? obj->pickupSound->formID : 0);
					logger::info("      >Putdown: {:0X}"sv, obj->putdownSound ? obj->putdownSound->formID : 0);
					logger::info("      >Attack: {:0X}"sv, obj->attackSound ? obj->attackSound->formID : 0);
					logger::info("      >Attack Loop: {:0X}"sv, obj->attackLoopSound ? obj->attackLoopSound->formID : 0);
					logger::info("      >Attack Fail: {:0X}"sv, obj->attackFailSound ? obj->attackFailSound->formID : 0);
					logger::info("      >Idle: {:0X}"sv, obj->idleSound ? obj->idleSound->formID : 0);
					logger::info("      >Equip: {:0X}"sv, obj->equipSound ? obj->equipSound->formID : 0);
					logger::info("      >UnEquip: {:0X}"sv, obj->unequipSound ? obj->unequipSound->formID : 0);
				}
			}
		}
		readData = std::move(filteredData);
		logger::info("Finished; Applied {} patches."sv, readData.size());
	}
}