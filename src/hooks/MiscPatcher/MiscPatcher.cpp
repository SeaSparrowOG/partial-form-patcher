#include "MiscPatcher.h"

#include "Settings/INI/INISettings.h"

namespace Hooks::MiscPatcher
{
	bool ListenForMisc() {
		logger::info("  Installing Misc Object Patcher..."sv);
		bool result = true;
		auto shouldInstall = Settings::INI::GetSetting<bool>(Settings::INI::GENERAL_MISC).value_or(false);
		if (!shouldInstall) {
			logger::info("    >User opted to not install the patcher."sv);
			return result;
		}
		result &= Misc::Listen();
		return result;
	}

	bool Misc::Listen() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESObjectMISC::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, Load);
		return true;
	}

	inline bool Misc::Load(RE::TESObjectMISC* a_this, RE::TESFile* a_file) {
		bool result = _load(a_this, a_file);
		return result;
	}
}