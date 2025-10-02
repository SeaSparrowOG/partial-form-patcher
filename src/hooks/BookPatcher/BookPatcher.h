#pragma once

namespace Hooks
{
	namespace BookPatcher
	{
		bool ListenForBooks();

		struct Book
		{
			inline static bool Listen();
			inline static bool Load(RE::TESObjectBOOK* a_this, RE::TESFile* a_file);
			inline static REL::Relocation<decltype(Load)> _load;
		};
	}
}