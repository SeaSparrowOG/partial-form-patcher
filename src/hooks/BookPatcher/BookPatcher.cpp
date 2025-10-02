#include "BookPatcher.h"

#include "Settings/INI/INISettings.h"

namespace Hooks::BookPatcher
{
	bool ListenForBooks() {
		logger::info("  Installing Book Patcher..."sv);
		bool result = true;
		auto shouldInstall = Settings::INI::GetSetting<bool>(Settings::INI::GENERAL_BOOK).value_or(false);
		if (!shouldInstall) {
			logger::info("    >User opted to not install the patcher."sv);
			return result;
		}
		result &= Book::Listen();
		return result;
	}

	bool Book::Listen() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESObjectBOOK::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, Load);
		return true;
	}

	inline bool Book::Load(RE::TESObjectBOOK* a_this, RE::TESFile* a_file) {
		bool response = _load(a_this, a_file);
		return response;
	}
}