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
				RE::BSFixedString        baseModel{ "" };
				RE::BSFixedString        alternateModel{ "" };
				RE::FormID               baseInventoryModel{ 0 };
				RE::FormID               alternateInventoryModel{ 0 };

				std::uint32_t            baseAltTextureCount{ 0 };
				std::uint32_t            alternateAltTextureCount{ 0 };
				ALT_TEXTURE*             baseAltTextures{ nullptr };
				ALT_TEXTURE*             alternateAltTextures{ nullptr };

				std::uint16_t            baseNumAddons{ 0 };
				std::uint16_t            alternateNumAddons{ 0 };
				std::uint32_t*           baseAddons{ nullptr };
				std::uint32_t*           alternateAddons{ nullptr };
				RE::TESFile*             visualOwner{ nullptr };
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

			std::unordered_map<RE::FormID, CachedData> mappedData{};
		};
	}
}