#pragma once

namespace RE
{
	enum class STAGGER_MAGNITUDE : std::int32_t
	{
		kNone = 0x0,
		kMagnitudeMin = 0x0,
		kSmall = 0x1,
		kMedium = 0x2,
		kLarge = 0x3,
		kExtraLarge = 0x4,
		kMagnitudeMax = 0x4,
		kCount = 0x5
	};
}
