#pragma once

#include "RE/B/BSFixedString.h"
#include "RE/B/BSTArray.h"
#include "RE/B/BSTOptional.h"
#include "RE/C/CountdownTimer.h"
#include "RE/X/XPChangeData.h"

namespace RE
{
	class TESQuest;

	namespace nsHUDTypes
	{
		class ObjectiveData
		{
		public:
			// members
			BSFixedStringCS objectiveName;   // 00
			bool            completed;       // 08
			bool            orWithPrevious;  // 09
		};
		static_assert(sizeof(ObjectiveData) == 0x10);

		class NotificationInfo
		{
		public:
			// members
			BSFixedStringCS                     title;        // 00
			BSFixedStringCS                     prefix;       // 08
			BSFixedStringCS                     soundName;    // 10
			BSTArray<nsHUDTypes::ObjectiveData> objectives;   // 18
			BSFixedString                       messageType;  // 30
			TESQuest*                           quest;        // 38
			CountdownTimer                      waitTime;     // 40
			BSTOptional<XPChangeData>           xpChange;     // 58
		};
		static_assert(sizeof(NotificationInfo) == 0x70);
	}
}
