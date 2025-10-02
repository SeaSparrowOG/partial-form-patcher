#pragma once

namespace Hooks
{
	namespace EffectPatcher
	{
		bool ListenForEffects();

		struct Effect
		{
			inline static bool Listen();
			inline static bool Load(RE::EffectSetting* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(Load)> _load;
		};
	}
}