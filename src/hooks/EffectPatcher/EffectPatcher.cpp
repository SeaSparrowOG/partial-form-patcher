#include "EffectPatcher.h"

#include "Settings/INI/INISettings.h"

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
		result &= Effect::Listen();
		return result;
	}

	bool Effect::Listen() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::EffectSetting::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, Load);
		return true;
	}

	inline bool Effect::Load(RE::EffectSetting* a_this, RE::TESFile* a_file) {
		bool result = _load(a_this, a_file);
		return result;
	}
}