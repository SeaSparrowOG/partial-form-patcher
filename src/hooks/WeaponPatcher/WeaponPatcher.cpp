#include "WeaponPatcher.h"

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
		result &= Weapon::Listen();
		return result;
	}

	bool Weapon::Listen() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESObjectWEAP::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, Load);
		return true;
	}

	inline bool Weapon::Load(RE::TESObjectWEAP* a_this, RE::TESFile* a_file) {
		bool result = _load(a_this, a_file);
		return result;
	}
}