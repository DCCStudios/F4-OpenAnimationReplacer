#pragma once

#include "RE/B/BSFixedString.h"
#include "RE/B/BSTOptional.h"
#include "RE/X/XPChangeData.h"

namespace RE
{
	class BGSQuestObjective;
	class MapMarkerData;
	class TESQuest;

	class HUDNotificationEvent
	{
	public:
		// members
		BSFixedString             messageType;           // 00
		BSFixedStringCS           messageTitle;          // 08
		TESQuest*                 quest;                 // 10
		BGSQuestObjective*        objective;             // 18
		const MapMarkerData*      markerData;            // 20
		BSTOptional<XPChangeData> xpChange;              // 28
		bool                      suppressNotification;  // 3C
	};
	static_assert(sizeof(HUDNotificationEvent) == 0x40);
}
