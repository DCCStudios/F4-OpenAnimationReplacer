#pragma once

#include "RE/B/BSFixedString.h"

namespace RE
{
	namespace Movement
	{
		enum class SPEED_DIRECTION
		{
			kTranslation = 0x0,
			kLeft = 0x0,
			kRight = 0x1,
			kForward = 0x2,
			kBack = 0x3,
			kRotations = 0x4,
			kPitch = 0x4,
			kRoll = 0x5,
			kYaw = 0x6,
			kCount = 0x7
		};

		class MaxSpeeds
		{
		public:
			// members
			float speeds[7][4];  // 00
		};
		static_assert(sizeof(MaxSpeeds) == 0x70);

		class TypeData
		{
		public:
			// members
			BSFixedString typeName;         // 00
			MaxSpeeds     defaultData;      // 08
			float         floatHeight;      // 78
			float         flightAngleGain;  // 7C
		};
		static_assert(sizeof(TypeData) == 0x80);
	}
}
