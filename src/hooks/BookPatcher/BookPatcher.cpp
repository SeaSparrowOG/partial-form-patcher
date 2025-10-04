#include "BookPatcher.h"

#include "Settings/INI/INISettings.h"

namespace Hooks::BookPatcher
{
	using _GetFormEditorID = const char* (*)(std::uint32_t);

	inline static std::string GetEditorID(const RE::TESForm* a_form)
	{
		switch (a_form->GetFormType()) {
		case RE::FormType::Keyword:
		case RE::FormType::LocationRefType:
		case RE::FormType::Action:
		case RE::FormType::MenuIcon:
		case RE::FormType::Global:
		case RE::FormType::HeadPart:
		case RE::FormType::Race:
		case RE::FormType::Sound:
		case RE::FormType::Script:
		case RE::FormType::Navigation:
		case RE::FormType::Cell:
		case RE::FormType::WorldSpace:
		case RE::FormType::Land:
		case RE::FormType::NavMesh:
		case RE::FormType::Dialogue:
		case RE::FormType::Quest:
		case RE::FormType::Idle:
		case RE::FormType::AnimatedObject:
		case RE::FormType::ImageAdapter:
		case RE::FormType::VoiceType:
		case RE::FormType::Ragdoll:
		case RE::FormType::DefaultObject:
		case RE::FormType::MusicType:
		case RE::FormType::StoryManagerBranchNode:
		case RE::FormType::StoryManagerQuestNode:
		case RE::FormType::StoryManagerEventNode:
		case RE::FormType::SoundRecord:
			return a_form->GetFormEditorID();
		default:
		{
			static auto tweaks = REX::W32::GetModuleHandleW(L"po3_Tweaks");
			static auto func = reinterpret_cast<_GetFormEditorID>(REX::W32::GetProcAddress(tweaks, "GetFormEditorID"));
			if (func) {
				return func(a_form->formID);
			}
			return {};
		}
		}
	}

	inline static bool IsSubrecord(RE::TESFile* file, const char* fourcc) {
		const auto type = file->GetCurrentSubRecordType();
		return (type & 0xFF) == static_cast<uint8_t>(fourcc[0]) &&
			((type >> 8) & 0xFF) == static_cast<uint8_t>(fourcc[1]) &&
			((type >> 16) & 0xFF) == static_cast<uint8_t>(fourcc[2]) &&
			((type >> 24) & 0xFF) == static_cast<uint8_t>(fourcc[3]);
	}

	bool ListenForBooks() {
		logger::info("  Installing Book Patcher..."sv);
		bool result = true;
		auto shouldInstall = Settings::INI::GetSetting<bool>(Settings::INI::GENERAL_BOOK).value_or(false);
		if (!shouldInstall) {
			logger::info("    >User opted to not install the patcher."sv);
			return result;
		}
		result &= Book::Listen();
		return result;
	}

	void PerformDataLoadedOp()
	{
		auto* manager = BookCache::GetSingleton();
		if (manager) {
			manager->OnDataLoaded();
		}
	}

	bool Book::Listen() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESObjectBOOK::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, Load);
		return true;
	}

	inline bool Book::Load(RE::TESObjectBOOK* a_this, RE::TESFile* a_file) {
		bool response = _load(a_this, a_file);
		auto* manager = response && a_this ? BookCache::GetSingleton() : nullptr;
		if (manager) {
			manager->OnBookLoaded(a_this, a_file);
		}
		return response;
	}

	void BookCache::OnDataLoaded() {
		for (auto& [id, vector] : loadedBooks) {
			for (auto& [file, offset, fileTexture] : vector) {
				if (vector.size() < 2) {
					continue;
				}
				auto* book = RE::TESForm::LookupByID<RE::TESObjectBOOK>(id);
				CompareBook(book, file, offset, fileTexture);
			}
			vector.clear();
		}
		loadedBooks.clear();

		logger::info("  Reading from {} loaded books..."sv, mappedData.size());
		for (auto& [id, data] : mappedData) {
			if (!data.overwritten) {
				continue;
			}
			auto* book = RE::TESForm::LookupByID<RE::TESObjectBOOK>(id);
			if (!book) {
				continue;
			}

			if (data.audioOwner || data.visualOwner) {
				logger::info("    Patching {}"sv, book->GetName());
			}

			if (data.audioOwner) {
				book->pickupSound = data.alternatePickUpSound;
				book->putdownSound = data.alternatePutDownSound;
				logger::info("      >Updated sounds from {}"sv, data.audioOwner->GetFilename());
			}

			if (data.visualOwner) {
				book->model = data.alternateModel;
				book->inventoryModel = data.alternateInventoryModel;
				book->textures = data.alternateTextures;
				book->numTextures = 0;
				book->textures = nullptr;
				logger::info("      >Updated visuals from {}"sv, data.visualOwner->GetFilename());
			}
		}
		logger::info("  Finished. Applied {} patches."sv, mappedData.size());
	}

	void BookCache::OnBookLoaded(RE::TESObjectBOOK* a_book, RE::TESFile* a_file)
	{
		if (loadedBooks.contains(a_book->formID)) {
			loadedBooks.at(a_book->formID).push_back({a_file, a_book->fileOffset, a_book->textures });
		}
		else {
			loadedBooks[a_book->formID] = { {a_file, a_book->fileOffset, a_book->textures} };
		}
	}

	void BookCache::CompareBook(RE::TESObjectBOOK* a_book, RE::TESFile* a_file, uint32_t a_offset, RE::BSResource::ID* a_fileTexture) {
		if (!a_file->OpenTES(RE::NiFile::OpenMode::kReadOnly, true)) {
			logger::error("Failed to open file {}", a_file->GetFilename());
			return;
		}
		if (!a_file->Seek(a_offset)) {
			logger::error("Failed to seek for {} in {}."sv, a_book->GetName(), a_file->GetFilename());
			a_file->CloseTES(false);
			return;
		}
		if (a_file->GetFormType() != RE::TESObjectBOOK::FORMTYPE) {
			logger::error("Form in {} exists, but is not a book."sv, a_file->GetFilename());
			a_file->CloseTES(false);
			return;
		}
		auto bookID = a_book->GetFormID();
		if (a_file->currentform.formID != bookID) {
			logger::error("Form in {} exists and is a book, but is not the stored book."sv, a_file->GetFilename());
			a_file->CloseTES(false);
			return;
		}

		RE::TESObjectSTAT* fileInventoryModel = nullptr;
		RE::BSFixedString fileModelPath = "";
		RE::BGSSoundDescriptorForm* filePickupSound = nullptr;
		RE::BGSSoundDescriptorForm* filePutdownSound = nullptr;
		while (a_file->SeekNextSubrecord()) {
			
			if (IsSubrecord(a_file, "INAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					fileInventoryModel = RE::TESForm::LookupByID<RE::TESObjectSTAT>(retrieved);
				}
			}
			// Model
			else if (IsSubrecord(a_file, "MODL")) {
				std::string temp(a_file->actualChunkSize, '\0');
				if (a_file->ReadData(temp.data(), temp.size())) {
					fileModelPath = temp.c_str();
				}
			}
			// Pickup
			else if (IsSubrecord(a_file, "YNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					filePickupSound = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(retrieved);
				}
			}
			// Putdown
			else if (IsSubrecord(a_file, "ZNAM")) {
				RE::FormID retrieved = 0;
				if (a_file->ReadData(&retrieved, a_file->actualChunkSize)) {
					filePutdownSound = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(retrieved);
				}
			}
		}
		a_file->CloseTES(false);

		if (!mappedData.contains(bookID)) {
			auto data = CachedData();
			data.baseInventoryModel = fileInventoryModel;
			data.alternateInventoryModel = data.baseInventoryModel;
			data.baseModel = fileModelPath;
			data.alternateModel = data.baseModel;
			data.basePutDownSound = filePutdownSound;
			data.alternatePickUpSound = data.basePutDownSound;
			data.basePickUpSound = filePickupSound;
			data.alternatePutDownSound = data.basePickUpSound;
			data.alternateTextures = data.baseTextures;
			data.baseTextures = a_fileTexture;
			data.alternateTextures = data.baseTextures;
			mappedData.emplace(bookID, std::move(data));
			return;
		}

		auto& data = mappedData.at(bookID);
		data.overwritten |= data.audioOwner || data.visualOwner;

		bool previousIsAudioMaster = false;
		if (data.audioOwner) {
			for (auto* master : std::span(a_file->masterPtrs, a_file->masterCount)) {
				previousIsAudioMaster |= master == data.audioOwner;
			}
		}

		bool previousIsVisualMaster = false;
		if (data.visualOwner) {
			for (auto* master : std::span(a_file->masterPtrs, a_file->masterCount)) {
				previousIsVisualMaster |= master == data.visualOwner;
			}
		}

		bool updateVisuals = false;
		if (fileModelPath != data.baseModel ||
			fileInventoryModel != data.baseInventoryModel)
		{
			updateVisuals = true;
		}
		bool updateSounds = false;
		if ((filePickupSound && filePickupSound != data.basePickUpSound) ||
			(filePutdownSound && filePutdownSound != data.basePutDownSound))
		{
			updateSounds = true;
		}
		if (!updateVisuals && !updateSounds) {
			return;
		}

		//Updates overwritten.
		if ((updateSounds && !data.visualOwner) || 
			(updateVisuals && !data.audioOwner) || 
			(updateVisuals && updateSounds)) 
		{
			data.overwritten = false;
		}

		if (updateSounds) {
			data.alternatePickUpSound = filePickupSound;
			data.alternatePutDownSound = filePutdownSound;
			data.audioOwner = a_file;
		}

		if (updateVisuals) {
			data.alternateInventoryModel = fileInventoryModel;
			data.alternateModel = fileModelPath;
			data.visualOwner = a_file;
			data.alternateTextures = a_fileTexture;
		}
	}
}