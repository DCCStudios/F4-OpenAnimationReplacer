#pragma once

#include "RE/B/BSExtraData.h"

namespace RE
{
	class __declspec(novtable) ExtraRadioRepeater :
		public BSExtraData  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::ExtraRadioRepeater };
		static constexpr auto VTABLE{ VTABLE::ExtraRadioRepeater };
		static constexpr auto TYPE{ EXTRA_DATA_TYPE::kRadioRepeater };

		// members
		BSPointerHandle<TESObjectREFR, BSUntypedPointerHandle<21, 5>> transmitterToRepeat;  // 18
		float                                                         innerRadius;          // 1C
		float                                                         outerRadius;          // 20
		bool                                                          unlimitedRange;       // 24
	};
	static_assert(sizeof(ExtraRadioRepeater) == 0x28);
}
