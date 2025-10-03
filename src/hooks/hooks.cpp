#include "Hooks/hooks.h"

#include "ArmorAddonPatcher/ArmorAddonPatcher.h"
#include "BookPatcher/BookPatcher.h"
#include "EffectPatcher/EffectPatcher.h"
#include "MiscPatcher/MiscPatcher.h"
#include "SoulGemPatcher/SoulGemPatcher.h"
#include "WeaponPatcher/WeaponPatcher.h"

namespace Hooks {
	bool Install() {
		SECTION_SEPARATOR;
		logger::info("Installing hooks..."sv);
		bool success = true;
		success &= ArmorAddonPatcher::ListenForArmorAddons();
		success &= BookPatcher::ListenForBooks();
		success &= EffectPatcher::ListenForEffects();
		success &= MiscPatcher::ListenForMisc();
		success &= SoulGemPatcher::ListenForSoulGems();
		success &= WeaponPatcher::ListenForWeapons();
		return success;
	}

	void DataLoaded()
	{
		ArmorAddonPatcher::PerformDataLoadedOp();
		BookPatcher::PerformDataLoadedOp();
	}
}