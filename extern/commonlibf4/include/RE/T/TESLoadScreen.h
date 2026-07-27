#pragma once

#include "RE/B/BGSLocalizedString.h"
#include "RE/T/TESCondition.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESModel.h"

namespace RE
{
	class BGSTransform;
	class TESBoundObject;

	class __declspec(novtable) TESLoadScreen :
		public TESForm  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::TESLoadScreen };
		static constexpr auto VTABLE{ VTABLE::TESLoadScreen };
		static constexpr auto FORM_ID{ ENUM_FORM_ID::kLSCR };

		class LoadNIFData
		{
		public:
			// members
			TESBoundObject* loadNif;                 // 00
			std::int16_t    rotationConstraints[2];  // 08
			TESModel        cameraPath;              // 10
			BGSTransform*   transform;               // 40
			float           zoomConstraints[2];      // 48
		};
		static_assert(sizeof(LoadNIFData) == 0x50);

		// members
		TESCondition       conditions;   // 20
		LoadNIFData*       loadNIFData;  // 28
		BGSLocalizedString loadingText;  // 30
	};
	static_assert(sizeof(TESLoadScreen) == 0x38);
}
