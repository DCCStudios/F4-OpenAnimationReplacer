#pragma once

#include "RE/B/BSExtraData.h"
#include "RE/R/RADIO_DATA.h"

namespace RE
{
	class __declspec(novtable) ExtraRadioData :
		public BSExtraData  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::ExtraRadioData };
		static constexpr auto VTABLE{ VTABLE::ExtraRadioData };
		static constexpr auto TYPE{ EXTRA_DATA_TYPE::kRadioData };

		// members
		RADIO_DATA data;  // 18
	};
	static_assert(sizeof(ExtraRadioData) == 0x28);
}
