#pragma once

namespace RE
{
	enum class ImpactResult : std::int32_t
	{
		kNone = 0x0,
		kDestroy = 0x1,
		kBounce = 0x2,
		kImpale = 0x3,
		kStick = 0x4
	};
}
