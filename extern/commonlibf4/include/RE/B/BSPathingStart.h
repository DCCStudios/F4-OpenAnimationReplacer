#pragma once

#include "RE/B/BSPathingLocation.h"

namespace RE
{
	class BSPathingStart
	{
	public:
		enum class START_TANGENT_TYPE
		{
			kNone = 0x0,
			kSet = 0x1,
			kKeep = 0x2,
			kMask = 0x3
		};

		enum class FLAGS
		{
			kSelected = 0x0,
			kStartsMoving = 0x1
		};

		// members
		BSPathingLocation location;  // 00
		float             tangent;   // 30
	};
	static_assert(sizeof(BSPathingStart) == 0x38);
}
