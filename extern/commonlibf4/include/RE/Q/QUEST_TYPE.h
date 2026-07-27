#pragma once

namespace RE
{
	enum class QUEST_TYPE : std::int32_t
	{
		kNone = 0x0,
		kMainquest = 0x1,
		kBrotherhood = 0x2,
		kInstitue = 0x3,
		kMinutemen = 0x4,
		kRailroad = 0x5,
		kMiscellaneous = 0x6,
		kSidequests = 0x7,
		kDLC01 = 0x8,
		kDLC02 = 0x9,
		kDLC03 = 0xA
	};
}
