#pragma once

#include "RE/D/DetectionFormulaData.h"
#include "RE/D/DetectionLevels.h"
#include "RE/N/NiPoint3.h"

namespace RE
{
	enum class DETECTION_TYPE;

	class DetectionData :
		public DetectionFormulaData  // 00
	{
	public:
		// members
		NiPoint3        detectedLocation;  // 74
		DetectionLevels detectionLevel;    // 80
		DETECTION_TYPE  detectionType;     // 84
	};
	static_assert(sizeof(DetectionData) == 0x88);
}
