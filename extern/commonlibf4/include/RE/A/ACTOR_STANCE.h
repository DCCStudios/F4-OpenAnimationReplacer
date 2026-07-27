#pragma once

namespace RE
{
	enum class ACTOR_STANCE : std::uint32_t
	{
		kNormal = 0x0,
		kSneaking = 0x1,
		kCoverVeryLow = 0x2,
		kCoverLow = 0x3,
		kCoverMid = 0x4,
		kCoverHigh = 0x5,
		kCount = 0x6
	};
}
