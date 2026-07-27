#pragma once

namespace RE
{
	enum class ACTOR_AGGRESSION : std::uint32_t
	{
		kCalmed = 0xFFFFFFFF,
		kUnagressive = 0x0,
		kAggressive = 0x1,
		kVeryAggressive = 0x2,
		kFrenzied = 0x3,
		kMax = 0x3
	};
}
