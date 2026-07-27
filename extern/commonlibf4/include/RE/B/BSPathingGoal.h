#pragma once

#include "RE/B/BSPathingLocation.h"
#include "RE/N/NiPoint3.h"

namespace RE
{
	class BSPathingGoal
	{
	public:
		enum class GOAL_RADIUS_TYPE
		{
			kNone = 0x0,
			kSoft = 0x1,
			kHard = 0x2,
			kMask = 0x3
		};

		enum class GOAL_HEIGHT_TYPE
		{
			kNone = 0x0,
			kFixed = 0x10,
			kMask = 0x10
		};

		// members
		NiPoint3          targetPoint;            // 00
		float             targetAngleTolerance;   // 0C
		BSPathingLocation location;               // 10
		float             zDelta;                 // 40
		float             goalRadius;             // 44
		float             normalizedSpeedAtGoal;  // 48
	};
	static_assert(sizeof(BSPathingGoal) == 0x50);
}
