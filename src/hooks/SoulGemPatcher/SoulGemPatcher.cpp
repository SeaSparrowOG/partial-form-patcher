#include "SoulGemPatcher.h"

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
		result &= SoulGem::Listen();
		return result;
	}

	bool SoulGem::Listen() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESSoulGem::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, Load);
		return true;
	}

	inline bool SoulGem::Load(RE::TESSoulGem* a_this, RE::TESFile* a_file) {
		bool result = _load(a_this, a_file);
		return result;
	}
}