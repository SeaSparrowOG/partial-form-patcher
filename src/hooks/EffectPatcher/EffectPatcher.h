#pragma once

namespace Hooks
{
	namespace EffectPatcher
	{
		bool ListenForEffects();
		void PerformDataLoadedOp();

		struct Effect
		{
			inline static bool HookEffectSetting();
			inline static bool LoadEffectSetting(RE::EffectSetting* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(LoadEffectSetting)> _load;
			inline static bool _loadedAll{ false };
		};

		class EffectCache : public REX::Singleton<EffectCache>
		{
		public:
			void OnEffectSettingLoaded(RE::EffectSetting* a_obj, RE::TESFile* a_file);
			void ReapplyChanges(RE::EffectSetting* a_obj);
			void PatchEffectSettings();

		private:
			struct ReadData
			{
				RE::FormID baseLight{ 0 };
				RE::FormID altLight{ 0 };
				RE::FormID baseEffectShader{ 0 };
				RE::FormID altEffectShader{ 0 };
				RE::FormID baseEnchantShader{ 0 };
				RE::FormID altEnchantShader{ 0 };
				RE::FormID baseCastingArt{ 0 };
				RE::FormID altCastingArt{ 0 };
				RE::FormID baseHitEffectArt{ 0 };
				RE::FormID altHitEffectArt{ 0 };
				RE::FormID baseImpactDataSet{ 0 };
				RE::FormID altImpactDataSet{ 0 };
				RE::FormID baseEnchantEffectArt{ 0 };
				RE::FormID altEnchantEffectArt{ 0 };
				RE::FormID baseHitVisuals{ 0 };
				RE::FormID altHitVisuals{ 0 };
				RE::FormID baseEnchantVisuals{ 0 };
				RE::FormID altEnchantVisuals{ 0 };
				RE::FormID baseImageSpaceMod{ 0 };
				RE::FormID altImageSpaceMod{ 0 };

				RE::FormID baseChargingSound{ 0 };
				RE::FormID altChargingSound{ 0 };
				RE::FormID baseReadyLoopSound{ 0 };
				RE::FormID altReadyLoopSound{ 0 };
				RE::FormID baseReleaseSound{ 0 };
				RE::FormID altReleaseSound{ 0 };
				RE::FormID baseCastLoopSound{ 0 };
				RE::FormID altCastLoopSound{ 0 };


				std::string_view visualOwner{ "" };
				std::string_view audioOwner{ "" };
				bool overwritten{ false };
				bool holdsData{ false };
			};

			std::unordered_map<RE::FormID, ReadData> readData{};
		};
	}
}