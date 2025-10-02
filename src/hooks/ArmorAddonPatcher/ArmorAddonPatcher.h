#pragma once

namespace Hooks
{
	namespace ArmorAddonPatcher
	{
		bool ListenForArmorAddons();
		void PerformDataLoadedOp();

		struct ArmorAddon
		{
			inline static bool Listen();
			inline static bool Load(RE::TESObjectARMA* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(Load)> _load;
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
				std::string baseModel_m{ "" };
				std::string altModel_m{ "" };
				std::string baseModel_f{ "" };
				std::string altModel_f{ "" };
				RE::BSTArray<RE::TESRace*> associatedRaceArray{};

				RE::BGSFootstepSet* baseFootstepSound{ nullptr };
				RE::BGSFootstepSet* altFootstepSound{ nullptr };

				RE::TESFile* modelOwningFile{ nullptr };
				RE::TESFile* soundOwningFile{ nullptr };
				bool requiresProcessing{ false };
			};

			std::unordered_map<RE::FormID, CachedData> mappedData{};
		};
	}
}