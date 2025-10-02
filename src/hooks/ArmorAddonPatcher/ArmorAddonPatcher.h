#pragma once

namespace Hooks
{
	namespace ArmorAddonPatcher
	{
		bool ListenForArmorAddons();

		struct ArmorAddon
		{
			inline static bool Listen();
			inline static bool Load(RE::TESObjectARMA* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(Load)> _load;
		};
	}
}