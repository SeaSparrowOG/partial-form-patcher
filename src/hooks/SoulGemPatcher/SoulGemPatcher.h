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
			void OnSoulGemLoaded(RE::TESSoulGem* a_soulGem, RE::TESFile* a_file);
			void OnDataLoaded();
				
		private:
			struct CachedData
			{
				// Start - Visuals
				RE::BSFixedString   baseModel{ "" };
				RE::BSFixedString   alternateModel{ "" };
				RE::BSResource::ID* baseTextures{};
				RE::BSResource::ID* alternateTextures{};
				uint16_t            alternateNumTextures{ 0 };
				RE::TESFile* visualOwner{ nullptr };
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

			struct ReadSoulGemData
			{
				RE::TESFile* file{ nullptr };
				uint32_t     fileOffset{ 0 };
				uint16_t     fileNumTextures{ 0 };
				RE::BSResource::ID* fileTextures{ nullptr };
			};

			void CompareSoulGem(RE::TESSoulGem* a_soulGem, ReadSoulGemData a_fileData);

			std::unordered_map<RE::FormID, std::vector<ReadSoulGemData>> readData{};
			std::unordered_map<RE::FormID, CachedData>                changedData{};
		};
	}
}