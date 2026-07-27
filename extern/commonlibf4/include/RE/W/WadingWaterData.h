#pragma once

namespace RE
{
	class WadingWaterData
	{
	public:
		// members
		NiPoint2 lastPlaceOffset;
		NiPoint2 lastDisplaceOffset;
		NiPoint3 lastPosition;
	};
	static_assert(sizeof(WadingWaterData) == 0x1C);
}
