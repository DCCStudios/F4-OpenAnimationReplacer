#pragma once

#include "RE/A/ACTOR_LOS_LOCATION.h"
#include "RE/A/AITimeStamp.h"

namespace RE
{
	class DetectionFormulaData
	{
	public:
		// members
		ACTOR_LOS_LOCATION losLocation;              // 00
		AITimeStamp        timeStamp;                // 04
		std::uint32_t      ambush;                   // 08
		std::uint32_t      targetRaceSize;           // 0C
		std::uint32_t      targetActionSound;        // 10
		float              lightLevel;               // 14
		float              modifiedLightLevel;       // 18
		float              visualDetectionDistance;  // 1C
		float              soundDetectionDistance;   // 20
		float              perception;               // 24
		float              basePerception;           // 28
		float              blindness;                // 2C
		float              deafness;                 // 30
		float              targetDistance;           // 34
		float              targetAngle;              // 38
		float              targetVerticalAngle;      // 3C
		float              targetAgility;            // 40
		float              targetSneakSkill;         // 44
		float              targetBaseStealth;        // 48
		float              targetEquippedWeight;     // 4C
		float              targetMovementNoiseMult;  // 50
		float              targetStealth;            // 54
		float              targetVisibility;         // 58
		float              visualDetectionLevel;     // 5C
		float              soundDetectionLevel;      // 60
		bool               LOS;                      // 64
		bool               LOS360;                   // 65
		bool               exterior;                 // 66
		bool               alert;                    // 67
		bool               sleeping;                 // 68
		bool               nightEye;                 // 69
		bool               targetMoving;             // 6A
		bool               targetRunning;            // 6B
		bool               targetSneaking;           // 6C
		bool               targetInvisible;          // 6D
		bool               targetInCover;            // 6E
		bool               targetObserved;           // 6F
		bool               combatTarget;             // 70
		bool               flying;                   // 71
	};
	static_assert(sizeof(DetectionFormulaData) == 0x74);
}
