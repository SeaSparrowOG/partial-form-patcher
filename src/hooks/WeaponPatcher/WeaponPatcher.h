#pragma once

namespace Hooks
{
	namespace WeaponPatcher
	{
		bool ListenForWeapons();

		struct Weapon
		{
			inline static bool Listen();
			inline static bool Load(RE::TESObjectWEAP* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(Load)> _load;
		};
	}
}