#pragma once

#include "RE/M/MARKER_TYPE.h"
#include "RE/M/MarkerIconTypes.h"
#include "RE/R/RelativeMarkerHeight.h"

namespace RE
{
	enum class MapMarkerSubCategories : std::int32_t;

	class __declspec(novtable) alignas(0x04) HUDMarkerData
	{
	public:
		// members
		BSPointerHandle<TESObjectREFR, BSUntypedPointerHandle<21, 5>> marketTarget;             // 00
		NiPoint3                                                      markerLocation;           // 04
		NiPoint2                                                      movieCoords;              // 10
		float                                                         heading;                  // 18
		float                                                         distanceToPlayerSq;       // 1C
		float                                                         markerDisplayDistance;    // 20
		MarkerIconTypes                                               markerIconType;           // 24
		RelativeMarkerHeight                                          relativeMarkerHeight;     // 28
		MapMarkerSubCategories                                        mapMarkerSubCategories;   // 2C
		MARKER_TYPE                                                   mapMarkerType;            // 30
		bool                                                          showFloatingQuestMarker;  // 34
		bool                                                          hostile;                  // 35
	};
	static_assert(sizeof(HUDMarkerData) == 0x38);
}
