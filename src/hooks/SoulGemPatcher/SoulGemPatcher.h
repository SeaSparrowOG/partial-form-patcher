#pragma once

namespace Hooks
{
	namespace SoulGemPatcher
	{
		bool ListenForSoulGems();
		void PerformDataLoadedOp();

		struct SoulGem
		{
			inline static bool HookSoulGem();
			inline static bool LoadSoulGemFromFile(RE::TESSoulGem* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(LoadSoulGemFromFile)> _load;
		};

		class SoulGemCache :
			public REX::Singleton<SoulGemCache>
		{
		public:
			void OnSoulGemLoaded(RE::TESSoulGem * a_obj, RE::TESFile* a_file);
			void OnDataLoaded();
				
		private:
			struct ReadData
			{
				RE::BSFixedString   baseModel{ "" };
				RE::BSFixedString   altModel{ "" };

				RE::FormID          basePutDownSound{ 0 };
				RE::FormID          basePickUpSound{ 0 };
				RE::FormID          altPutDownSound{ 0 };
				RE::FormID          altPickUpSound{ 0 };

				std::string_view visualOwner{ "" };
				std::string_view audioOwner{ "" };
				bool overwritten{ false };
				bool holdsData{ false };
			};

			std::unordered_map<RE::FormID, ReadData> readData{};
		};
	}
}