#pragma once

#include "RE/H/hkBool.h"
#include "RE/H/hknpConstraintId.h"

namespace RE
{
	class hknpConstraint;
	class hknpWorld;

	class hknpConstraintManager
	{
	public:
		// members
		hknpWorld*       world;                        // 0x00
		hknpConstraint*  constraintBuffer;             // 0x08
		std::uint32_t    capacity;                     // 0x10
		hkBool           constraintBufferIsUserOwned;  // 0x14
		hknpConstraintId firstDestroyedConstraintId;   // 0x18
		hknpConstraintId firstFreeConstraintId;        // 0x1C
		std::uint32_t    numAllocatedConstraints;      // 0x20
		std::uint32_t    numDestroyedConstraints;      // 0x24
		hknpConstraintId peakConstraintId;             // 0x28
	};
	static_assert(sizeof(hknpConstraintManager) == 0x30);
}
