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
}