#pragma once

namespace Hooks
{
	namespace MiscPatcher
	{
		bool ListenForMisc();

		struct Misc
		{
			inline static bool Listen();
			inline static bool Load(RE::TESObjectMISC* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(Load)> _load;
		};
	}
}