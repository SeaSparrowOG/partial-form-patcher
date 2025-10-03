#include "BookPatcher.h"

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
		logger::info("  Reading from {} loaded books..."sv, mappedData.size());
		std::unordered_map<RE::FormID, CachedData> newData{};
		for (auto& [id, data] : mappedData) {
			if (!data.overwritten) {
				continue;
			}
			auto* book = RE::TESForm::LookupByID<RE::TESObjectBOOK>(id);
			if (!book) {
				logger::warn("    >Warning: Engine failed to load book with ID {}", id);
				continue;
			}
			if (data.audioOwner) {
				book->pickupSound = data.alternatePickUpSound;
				book->putdownSound = data.alternatePutDownSound;
				logger::info("    >Loaded book sounds for {} from {}.", book->GetName(), data.audioOwner->GetFilename());
			}
			if (data.visualOwner) {
				book->model = data.alternateModel;
				book->inventoryModel = RE::TESForm::LookupByID<RE::TESObjectSTAT>(data.alternateInventoryModel);
				book->alternateTextures = data.alternateAltTextures;
				book->numAddons = data.alternateNumAddons;
				book->addons = data.alternateAddons;
				logger::info("    >Loaded book visuals for {} from {}.", book->GetName(), data.visualOwner->GetFilename());
			}
			newData.emplace(id, std::move(data));
		}
		mappedData = newData;
		logger::info("  Finished. Applied {} patches."sv, mappedData.size());
	}

	void BookCache::OnBookLoaded(RE::TESObjectBOOK* a_book, RE::TESFile* a_file) {
		auto bookID = a_book->GetFormID();
		if (!mappedData.contains(bookID)) {
			auto data = CachedData();
			data.baseModel = a_book->model;
			data.baseAddons = a_book->addons;
			data.baseNumAddons = a_book->numAddons;
			data.baseAltTextureCount = a_book->numAlternateTextures;
			data.baseAltTextures = a_book->alternateTextures;
			data.baseInventoryModel = a_book->inventoryModel ? a_book->inventoryModel->formID : 0;
			data.basePickUpSound = a_book->pickupSound;
			data.basePutDownSound = a_book->putdownSound;
			mappedData.emplace(bookID, std::move(data));
			return;
		}
		LOG_DEBUG("Reading data for book: {}", a_book->GetName());
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
		if (previousIsVisualMaster ||
			a_book->model != data.baseModel ||
			a_book->addons != data.baseAddons ||
			a_book->inventoryModel != RE::TESForm::LookupByID<RE::TESObjectSTAT>(data.baseInventoryModel)
		)
		{
			updateVisuals = true;
		}
		bool updateSounds = false;
		if (previousIsAudioMaster ||
			a_book->pickupSound != data.basePickUpSound ||
			a_book->putdownSound != data.basePutDownSound
		)
		{
			updateSounds = true;
		}
		if (!updateVisuals && !updateSounds) {
			return;
		}
		if (updateSounds && !data.visualOwner) {
			data.overwritten = false;
		}
		else if (updateVisuals && !data.audioOwner) {
			data.overwritten = false;
		}
		else if (updateVisuals && updateSounds) {
			data.overwritten = false;
		}

		if (updateSounds) {
			data.audioOwner = a_file;
			data.alternatePickUpSound = a_book->pickupSound;
			data.alternatePutDownSound = a_book->putdownSound;
		}

		if (updateVisuals) {
			data.visualOwner = a_file;
			data.alternateModel = a_book->model;
			data.alternateInventoryModel = a_book->inventoryModel->formID;
			data.alternateAltTextureCount = a_book->numAlternateTextures;
			data.alternateAltTextures = a_book->alternateTextures;
			data.alternateAddons = a_book->addons;
			data.alternateNumAddons = a_book->numAddons;
		}
	}
}