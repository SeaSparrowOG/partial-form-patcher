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
		};

		class BookCache :
			public REX::Singleton<BookCache>
		{
		public:
			void OnBookLoaded(RE::TESObjectBOOK* a_book, RE::TESFile* a_file);
			void OnDataLoaded();

		private:
			using ALT_TEXTURE = RE::TESModelTextureSwap::AlternateTexture;
			struct CachedData
			{
				// Start - Visuals
				RE::BSFixedString   baseModel{ "" };
				RE::BSFixedString   alternateModel{ "" };
				RE::TESObjectSTAT*  baseInventoryModel{ 0 };
				RE::TESObjectSTAT*  alternateInventoryModel{ 0 };
				RE::BSResource::ID* baseTextures{};
				RE::BSResource::ID* alternateTextures{};
				uint16_t            alternateNumTextures{ 0 };
				RE::TESFile*        visualOwner{ nullptr };
				// End - Visuals

				// Start - Audio
				RE::BGSSoundDescriptorForm* basePickUpSound{ nullptr };
				RE::BGSSoundDescriptorForm* basePutDownSound{ nullptr };
				RE::BGSSoundDescriptorForm* alternatePickUpSound{ nullptr };
				RE::BGSSoundDescriptorForm* alternatePutDownSound{ nullptr };
				RE::TESFile* audioOwner{ nullptr };
				// End - Audio

				bool overwritten{ false };
			};

			struct ReadBookData
			{
				RE::TESFile* file{ nullptr };
				uint32_t     fileOffset{ 0 };
				uint16_t     fileNumTextures{ 0 };
				RE::BSResource::ID* fileTextures{ nullptr };
			};

			void CompareBook(RE::TESObjectBOOK* a_book, ReadBookData a_fileData);

			std::unordered_map<RE::FormID, std::vector<ReadBookData>> readData{};
			std::unordered_map<RE::FormID, CachedData>                changedData{};
		};
	}
}