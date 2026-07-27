#pragma once

#include "RE/B/BSExtraData.h"
#include "RE/T/TESObjectACTIDefs.h"

namespace RE
{
	class __declspec(novtable) ExtraRadioReceiver :
		public BSExtraData  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::ExtraRadioReceiver };
		static constexpr auto VTABLE{ VTABLE::ExtraRadioReceiver };
		static constexpr auto TYPE{ EXTRA_DATA_TYPE::kRadioReceiver };

		// members
		TESObjectACTIDefs::RadioReceiverData data;  // 18
	};
	static_assert(sizeof(ExtraRadioReceiver) == 0x30);
}
