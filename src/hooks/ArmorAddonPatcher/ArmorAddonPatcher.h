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
		};

		class ArmorAddonCache :
			public REX::Singleton<ArmorAddonCache>
		{
		public:
			void OnAddonLoaded(RE::TESObjectARMA* a_addon, RE::TESFile* a_file);
			void OnDataLoaded();

		private:
			struct CachedData
			{
				
				bool overwritten{ false };
			};

			std::unordered_map<RE::FormID, CachedData> changedData{};
		};
	}
}