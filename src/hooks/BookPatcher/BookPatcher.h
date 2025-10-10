#pragma once

namespace Hooks
{
	namespace BookPatcher
	{
		bool ListenForBooks();
		void PerformDataLoadedOp();

		struct Book
		{
			inline static bool HookTESObjectBOOK();
			inline static bool LoadBookFromFile(RE::TESObjectBOOK* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(LoadBookFromFile)> _load;
			inline static bool _loadedAll{ false };
		};

		class BookCache :
			public REX::Singleton<BookCache>
		{
		public:
			void OnBookLoaded(RE::TESObjectBOOK* a_obj, RE::TESFile* a_file);
			void ReapplyChanges(RE::TESObjectBOOK* a_obj);
			void PatchBookObjects();

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