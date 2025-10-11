#pragma once

namespace Hooks
{
	namespace ArmorAddonPatcher
	{
		bool ListenForArmorAddons();
		void PerformDataLoadedOp();

		struct ArmorAddon
		{
			inline static bool HookTESObjectARMA();
			inline static bool LoadAddonFromFile(RE::TESObjectARMA* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(LoadAddonFromFile)> _load;
			inline static bool _loadedAll{ false };
		};

		class ArmorAddonCache :
			public REX::Singleton<ArmorAddonCache>
		{
		public:
			void OnAddonLoaded(RE::TESObjectARMA* a_addon, RE::TESFile* a_file);
			void ReApplyChanges(RE::TESObjectARMA* a_addon);
			void OnDataLoaded();

		private:
			struct ReadData
			{
				RE::BSFixedString baseMaleBipedMode{ "" }; //MOD2
				RE::BSFixedString baseFemaleBipedMode{ "" }; //MOD3
				RE::BSFixedString baseMaleFirstPersonBipedModel{ "" }; //MOD4
				RE::BSFixedString baseFemaleFirstPersonBipedModel{ "" }; //MOD5

				RE::FormID baseMaleSkinTexture{ 0 }; //NAM0
				RE::FormID baseFemaleSkinTexture{ 0 }; //NAM1
				RE::FormID baseMaleSkinTextureSwapList{ 0 }; //NAM2
				RE::FormID baseFemaleSkinTextureSwapList{ 0 }; //NAM3

				RE::BSFixedString altMaleBipedModel{ "" };
				RE::BSFixedString altFemaleBipedModel{ "" };
				RE::BSFixedString altMaleFirstPersonBipedModel{ "" };
				RE::BSFixedString altFemaleFirstPersonBipedModel{ "" };

				RE::FormID altMaleSkinTexture{ 0 };
				RE::FormID altFemaleSkinTexture{ 0 };
				RE::FormID altMaleSkinTextureSwapList{ 0 };
				RE::FormID altFemaleSkinTextureSwapList{ 0 };

				RE::FormID baseFootstepSet{ 0 }; //SNDD
				RE::FormID altFootstepSet{ 0 };

				std::string_view visualOwner{ "" };
				std::string_view audioOwner{ "" };
				bool overwritten{ false };
			};

			std::unordered_map<RE::FormID, ReadData> readData{};
		};
	}
}