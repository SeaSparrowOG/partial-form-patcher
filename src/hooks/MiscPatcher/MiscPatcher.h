#pragma once

namespace Hooks
{
	namespace MiscPatcher
	{
		bool ListenForMisc();
		void PerformDataLoadedOp();

		struct Misc
		{
			inline static bool HookTESObjectMISC();
			inline static bool LoadMiscObject(RE::TESObjectMISC* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(LoadMiscObject)> _load;
			inline static bool _loadedAll{ false };
		};

		class MiscObjectCache : public REX::Singleton<MiscObjectCache>
		{
		public:
			void OnMiscObjectLoaded(RE::TESObjectMISC* a_obj, RE::TESFile* a_file);
			void ReapplyChanges(RE::TESObjectMISC* a_obj);
			void PatchMiscObjects();

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