#pragma once

namespace Hooks
{
	namespace SoulGemPatcher
	{
		bool ListenForSoulGems();

		struct SoulGem
		{
			inline static bool Listen();
			inline static bool Load(RE::TESSoulGem* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(Load)> _load;
		};
	}
}