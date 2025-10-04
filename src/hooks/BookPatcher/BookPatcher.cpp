#include "BookPatcher.h"

#include "common/Utilities.h"
#include "Settings/INI/INISettings.h"

namespace Hooks::BookPatcher
{
	bool ListenForBooks() {
		logger::info("  Installing Book Patcher..."sv);
		bool result = true;
		auto shouldInstall = Settings::INI::GetSetting<bool>(Settings::INI::GENERAL_BOOK).value_or(false);
		if (!shouldInstall) {
			logger::info("    >User opted to not install the patcher."sv);
			return result;
		}
		result &= Book::HookTESObjectBOOK();
		return result;
	}

	void PerformDataLoadedOp()
	{
		auto* manager = BookCache::GetSingleton();
		if (manager) {
			manager->OnDataLoaded();
		}
	}

	bool Book::HookTESObjectBOOK() {
		REL::Relocation<std::uintptr_t> VTABLE{ RE::TESObjectBOOK::VTABLE[0] };
		_load = VTABLE.write_vfunc(0x6, LoadBookFromFile);
		return true;
	}

	inline bool Book::LoadBookFromFile(RE::TESObjectBOOK* a_this, RE::TESFile* a_file) {
		bool response = _load(a_this, a_file);
		auto* manager = response && a_this ? BookCache::GetSingleton() : nullptr;
		if (manager) {
			manager->OnBookLoaded(a_this, a_file);
		}
		return response;
	}

	void BookCache::OnDataLoaded() {
		logger::info("  Processing {} books..."sv, readData.size());
		for (auto& [id, data] : readData) {
			if (data.size() < 2) {
				continue;
			}

			auto* book = RE::TESForm::LookupByID<RE::TESObjectBOOK>(id);
			if (!book) {
				logger::warn("    >Warning, book with FormID {:0X} is no longer a book in memory. Skipping, but take note."sv, id);
				continue;
			}

			for (auto& fileData : data) {
				CompareBook(book, std::move(fileData));
			}
		}
		readData.clear();

		std::unordered_map<RE::FormID, CachedData> actualChanges{};
		for (auto& [id, data] : changedData) {
			if (!data.overwritten) {
				continue;
			}
			auto* book = RE::TESForm::LookupByID<RE::TESObjectBOOK>(id);
			if (!book) {
				continue;
			}

			auto* bookName = book->GetName();
			if (strcmp(bookName, "") == 0) {
				bookName = "[No Name]";
			}
			auto bookEDID = Utilities::GetEditorID(book);
			bookEDID = bookEDID.empty() ? "Couldn't Retrieve EditorID" : bookEDID;
			logger::info("    >Patching {} ({:0X} -> {})", bookName, id, bookEDID);

			if (data.audioOwner) {
				book->pickupSound = data.alternatePickUpSound;
				book->putdownSound = data.alternatePutDownSound;
				logger::info("      Forwarded {:0X} and {:0X} from {}."sv,
					book->pickupSound ? book->pickupSound->formID : 0,
					book->putdownSound ? book->putdownSound->formID : 0,
					data.audioOwner->GetFilename());
			}

			if (data.visualOwner) {
				book->textures = data.alternateTextures;
				book->numTextures = data.alternateNumTextures;
				book->model = data.alternateModel;
				book->inventoryModel = data.alternateInventoryModel;
				logger::info("      Forwarded visual data from {}."sv, data.visualOwner->GetFilename());
			}
			actualChanges.emplace(id, std::move(data));
		}
		changedData.clear();
		changedData = actualChanges;
		actualChanges.clear();
		logger::info("  Finished processing {} patches for books.", changedData.size());
	}

	void BookCache::OnBookLoaded(RE::TESObjectBOOK* a_book, RE::TESFile* a_file) {
		auto bookID = a_book->formID;
		auto data = ReadBookData();
		data.file = a_file;
		data.fileOffset = a_book->fileOffset;
		data.fileTextures = a_book->textures;
		data.fileNumTextures = a_book->numTextures;
		if (readData.contains(bookID)) {
			readData.at(bookID).push_back(std::move(data));
		}
		else {
			auto newVec = std::vector<ReadBookData>();
			newVec.push_back(std::move(data));
			readData.emplace(bookID, std::move(newVec));
		}
	}

	void BookCache::CompareBook(RE::TESObjectBOOK* a_book, ReadBookData a_fileData) {
		auto* file = a_fileData.file;
		auto offset = a_fileData.fileOffset;
		auto* fileTextures = a_fileData.fileTextures;

		if (!file->OpenTES(RE::NiFile::OpenMode::kReadOnly, true)) {
			logger::error("Failed to open file {}", file->GetFilename());
			return;
		}
		if (!file->Seek(offset)) {
			logger::error("Failed to seek for {} in {}."sv, a_book->GetName(), file->GetFilename());
			file->CloseTES(false);
			return;
		}
		if (file->GetFormType() != RE::TESObjectBOOK::FORMTYPE) {
			logger::error("Form in {} exists, but is not a book."sv, file->GetFilename());
			file->CloseTES(false);
			return;
		}
		auto bookID = a_book->GetFormID();
		if (file->currentform.formID != bookID) {
			logger::error("Form in {} exists and is a book, but is not the stored book."sv, file->GetFilename());
			file->CloseTES(false);
			return;
		}

		RE::TESObjectSTAT* fileInventoryModel = nullptr;
		RE::BSFixedString fileModelPath = "";
		RE::BGSSoundDescriptorForm* filePickupSound = nullptr;
		RE::BGSSoundDescriptorForm* filePutdownSound = nullptr;
		while (file->SeekNextSubrecord()) {
			
			if (Utilities::IsSubrecord(file, "INAM")) {
				RE::FormID retrieved = 0;
				if (file->ReadData(&retrieved, file->actualChunkSize)) {
					fileInventoryModel = RE::TESForm::LookupByID<RE::TESObjectSTAT>(retrieved);
				}
			}
			// Model
			else if (Utilities::IsSubrecord(file, "MODL")) {
				std::string temp(file->actualChunkSize, '\0');
				if (file->ReadData(temp.data(), temp.size())) {
					fileModelPath = temp.c_str();
				}
			}
			// Pickup
			else if (Utilities::IsSubrecord(file, "YNAM")) {
				RE::FormID retrieved = 0;
				if (file->ReadData(&retrieved, file->actualChunkSize)) {
					filePickupSound = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(retrieved);
				}
			}
			// Putdown
			else if (Utilities::IsSubrecord(file, "ZNAM")) {
				RE::FormID retrieved = 0;
				if (file->ReadData(&retrieved, file->actualChunkSize)) {
					filePutdownSound = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(retrieved);
				}
			}
		}
		file->CloseTES(false);

		if (!changedData.contains(a_book->formID)) {
			auto newChangedData = CachedData();
			newChangedData.baseInventoryModel = fileInventoryModel;
			newChangedData.baseModel = fileModelPath.empty() ? a_book->model : fileModelPath;
			newChangedData.basePickUpSound = filePickupSound;
			newChangedData.basePutDownSound = filePutdownSound;
			newChangedData.baseTextures = fileTextures;

			changedData.emplace(a_book->formID, std::move(newChangedData));
			return;
		}

		auto& data = changedData.at(bookID);
		data.overwritten |= data.audioOwner || data.visualOwner;

		bool previousIsAudioMaster = false;
		if (data.audioOwner) {
			for (auto* master : std::span(a_fileData.file->masterPtrs, a_fileData.file->masterCount)) {
				previousIsAudioMaster |= master == data.audioOwner;
			}
		}

		bool previousIsVisualMaster = false;
		if (data.visualOwner) {
			for (auto* master : std::span(a_fileData.file->masterPtrs, a_fileData.file->masterCount)) {
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
			data.audioOwner = a_fileData.file;
		}

		if (updateVisuals) {
			data.alternateInventoryModel = fileInventoryModel;
			data.alternateModel = fileModelPath;
			data.visualOwner = a_fileData.file;
			data.alternateTextures = a_fileData.fileTextures;
			data.alternateNumTextures = a_fileData.fileNumTextures;
		}
	}
}