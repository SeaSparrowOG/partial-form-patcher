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

	bool ArmorAddon::Listen() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESObjectARMA::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, Load);
		return true;
	}

	inline bool ArmorAddon::Load(RE::TESObjectARMA* a_this, RE::TESFile* a_file) {
		bool response = _load(a_this, a_file);
		return response;
	}
}