#pragma once

namespace RE
{
	enum class RelativeMarkerHeight : std::int32_t
	{
		kInvalid = 0x0,
		kBelowPlayer = 0x1,
		kLevelWithPlayer = 0x2,
		kAbovePlayer = 0x3
	};
}
