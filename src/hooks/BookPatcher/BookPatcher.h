#pragma once

namespace Hooks
{
	namespace BookPatcher
	{
		bool ListenForBooks();
		void PerformDataLoadedOp();

		struct Book
		{
			inline static bool Listen();
			inline static bool Load(RE::TESObjectBOOK* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(Load)> _load;
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
				RE::BSResource::ID*      baseTextures{};
				RE::BSResource::ID*      alternateTextures{};
				RE::TESFile*        visualOwner{ nullptr };
				// End - Visuals

				// Start - Audio
				RE::BGSSoundDescriptorForm* basePickUpSound{ nullptr };
				RE::BGSSoundDescriptorForm* basePutDownSound{ nullptr };
				RE::BGSSoundDescriptorForm* alternatePickUpSound{ nullptr };
				RE::BGSSoundDescriptorForm* alternatePutDownSound{ nullptr };
				RE::TESFile* audioOwner{ nullptr };
				// End - Audio

				// Overwritten conditions: Either audioOwner or visualOwner is defined 
				// by the time this form would be loaded from another plugin.
				bool overwritten{ false };
			};

			struct ObjectReadData
			{
				RE::TESFile* file{ nullptr };
				uint32_t     fileOffset{ 0 };
				RE::BSResource::ID* fileTextures{ nullptr };
			};

			void CompareBook(RE::TESObjectBOOK* a_book, RE::TESFile* a_file, uint32_t a_offset, RE::BSResource::ID* a_fileTexture);

			std::unordered_map<RE::FormID, CachedData>                  mappedData{};
			std::unordered_map<RE::FormID, std::vector<ObjectReadData>> loadedBooks{};
		};
	}
}