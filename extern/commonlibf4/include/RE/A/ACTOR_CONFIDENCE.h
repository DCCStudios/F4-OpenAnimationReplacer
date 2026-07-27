#pragma once

namespace RE
{
	enum class ACTOR_CONFIDENCE : std::uint32_t
	{
		kCowardly = 0x0,
		kCautious = 0x1,
		kAverage = 0x2,
		kBrave = 0x3,
		kFoolhardy = 0x4,
		kCount = 0x5
	};
}
