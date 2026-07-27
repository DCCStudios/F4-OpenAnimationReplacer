#pragma once

#include "RE/N/NiPlane.h"

namespace RE
{
	class NiFrustumPlanes
	{
	public:
		enum class Plane
		{
			kNearPlane = 0x0,
			kFarPlane = 0x1,
			kLeftPlane = 0x2,
			kRightPlane = 0x3,
			kTopPlane = 0x4,
			kBottomPlane = 0x5
		};

		// members
		NiPlane       cullingPlanes[6];  // 00
		std::uint32_t activePlanes;      // 60
		std::uint32_t basePlaneStates;   //64
		std::byte     pad[0x8];          // 68
	};
	static_assert(sizeof(NiFrustumPlanes) == 0x70);
}
