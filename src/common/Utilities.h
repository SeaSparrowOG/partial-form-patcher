#pragma once

namespace Utilities
{
	inline static std::string GetEditorID(const RE::TESForm* a_form)
	{
		using _GetFormEditorID = const char* (*)(std::uint32_t);
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

	inline static std::string RecordType(const uint32_t a_fourCC) {
		char str[5];
		str[0] = static_cast<char>(a_fourCC & 0xFF);
		str[1] = static_cast<char>((a_fourCC >> 8) & 0xFF);
		str[2] = static_cast<char>((a_fourCC >> 16) & 0xFF);
		str[3] = static_cast<char>((a_fourCC >> 24) & 0xFF);
		str[4] = '\0';
		return std::string(str);
	}

	inline static bool IsSubrecord(RE::TESFile* file, const std::string& fourcc) {
		const auto type = file->GetCurrentSubRecordType();
		return (type & 0xFF) == static_cast<uint8_t>(fourcc[0]) &&
			((type >> 8) & 0xFF) == static_cast<uint8_t>(fourcc[1]) &&
			((type >> 16) & 0xFF) == static_cast<uint8_t>(fourcc[2]) &&
			((type >> 24) & 0xFF) == static_cast<uint8_t>(fourcc[3]);
	}
	
	inline static std::string GetFormattedName(RE::TESForm* form) {
		if (!form) {
			return "<null>";
		}
		auto id = form->formID;
		auto edid = GetEditorID(form);
		auto name = form->GetName();
		
		std::string response = "";
		std::string formattedID = fmt::format("{:08X}", id);
		if (!edid.empty()) {
			response = fmt::format("[{}] - {}", formattedID, edid);
		}
		else {
			response = fmt::format("[{}]", formattedID);
		}
		if (strcmp(name, "") != 0) {
			response = fmt::format("({}): {}", response, name);
		}
		return response;
	}

	inline static RE::FormID GetAbsoluteFormID(const RE::FormID a_formID, RE::TESFile* a_file)
	{
		if (!a_file) {
			return a_formID;
		}

		std::vector<RE::TESFile*> visitedFiles{};
		visitedFiles.reserve(a_file->masterCount);
		auto formIDIndex = static_cast<std::uint8_t>(0xFF & (a_formID >> 24));

		for (auto* master : std::span(a_file->masterPtrs, a_file->masterCount)) {
			visitedFiles.push_back(master);
		}

		if (formIDIndex >= visitedFiles.size()) {
			if (a_file->IsLight()) {
				return 0xFE000000 | ((a_file->smallFileCompileIndex & 0xFF) << 12) | (a_formID & 0xFFF);
			}
			return ((a_file->compileIndex & 0xFF) << 24) | (a_formID & 0xFFFFFF);
		}

		std::sort(std::begin(visitedFiles), std::end(visitedFiles),
			[](const RE::TESFile* lhs, const RE::TESFile* rhs) {
				return lhs->compileIndex < rhs->compileIndex;
			});

		auto* visited = visitedFiles.at(formIDIndex);
		if (visited->IsLight()) {
			return 0xFE000000 | ((visited->smallFileCompileIndex & 0xFF) << 12) | (a_formID & 0xFFF);
		}
		return ((visited->compileIndex & 0xFF) << 24) | (a_formID & 0xFFFFFF);
	}
}