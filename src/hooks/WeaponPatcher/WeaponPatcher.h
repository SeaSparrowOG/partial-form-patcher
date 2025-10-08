#pragma once

namespace Hooks
{
	namespace WeaponPatcher
	{
		bool ListenForWeapons();
		void PerformDataLoadedOp();

		struct Weapon
		{
			inline static bool HookTESObjectWEAP();
			inline static bool LoadWeaponFromFile(RE::TESObjectWEAP* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(LoadWeaponFromFile)> _load;
		};

		class WeaponCache :
			public REX::Singleton<WeaponCache>
		{
		public:
			void OnWeaponLoaded(RE::TESObjectWEAP* a_obj, RE::TESFile* a_file);
			void OnDataLoaded();

		private:
			struct ReadData
			{
				//Visual
				RE::BSFixedString   baseModel{ "" };
				RE::BSFixedString   altModel{ "" };
				RE::FormID          baseFirstPersonMesh{ 0 };
				RE::FormID          altFirstPersonMesh{ 0 };
				
				//Impact
				RE::FormID          baseImpactDataSet{ 0 };
				RE::FormID          altImpactDataSet{ 0 };
				RE::FormID          baseBlockBashImpactDataSet{ 0 };
				RE::FormID          altBlockBashImpactDataSet{ 0 };
				RE::FormID          baseBlockAlternateMaterial{ 0 };
				RE::FormID          altBlockAlternateMaterial{ 0 };

				// Audio
				RE::FormID          basePutDownSound{ 0 };
				RE::FormID          basePickUpSound{ 0 };
				RE::FormID          altPutDownSound{ 0 };
				RE::FormID          altPickUpSound{ 0 };
				RE::FormID          baseAttackSound{ 0 };
				RE::FormID          altAttackSound{ 0 };
				RE::FormID          baseAttackLoopSound{ 0 };
				RE::FormID          altAttackLoopSound{ 0 };
				RE::FormID          baseAttackFailSound{ 0 };
				RE::FormID          altAttackFailSound{ 0 };
				RE::FormID          baseIdleSound{ 0 };
				RE::FormID          altIdleSound{ 0 };
				RE::FormID          baseEquipSound{ 0 };
				RE::FormID          altEquipSound{ 0 };
				RE::FormID          baseUnEquipSound{ 0 };
				RE::FormID          altUnEquipSound{ 0 };

				// Usage
				std::string_view visualOwner{ "" };
				std::string_view impactOwner{ "" };
				std::string_view audioOwner{ "" };
				bool overwritten{ false };
				bool holdsData{ false };
			};

			std::unordered_map<RE::FormID, ReadData> readData{};
		};
	}
}