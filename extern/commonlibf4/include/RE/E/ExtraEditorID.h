#pragma once

#include "RE/B/BSExtraData.h"

namespace RE
{
	class __declspec(novtable) ExtraEditorID :
		public BSExtraData  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::ExtraEditorID };
		static constexpr auto VTABLE{ VTABLE::ExtraEditorID };
		static constexpr auto TYPE{ EXTRA_DATA_TYPE::kEditorID };

		// members
		BSFixedString editorID;  // 18
	};
	static_assert(sizeof(ExtraEditorID) == 0x20);
}
