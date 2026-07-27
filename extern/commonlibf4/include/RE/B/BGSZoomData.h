#pragma once

#include "RE/N/NiPoint3.h"
#include "RE/T/TESForm.h"

namespace RE
{
	class __declspec(novtable) BGSZoomData :
		public TESForm  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::BGSZoomData };
		static constexpr auto VTABLE{ VTABLE::BGSZoomData };
		static constexpr auto FORM_ID{ ENUM_FORM_ID::kZOOM };

		enum class Overlay : std::uint32_t
		{
			kDefault = 0x0,
			kFine = 0x1,
			kDuplex = 02,
			kGerman = 0x3,
			kDot = 0x4,
			kMilDot = 0x5,
			kCircle = 0x6,
			kOldRangefind = 0x7,
			kModernRangefind = 0x8,
			kSvd = 0x9,
			kHandPainted = 0xA,
			kBinoculars = 0xB,
			kCross = 0xC,
			kDoubleZero = 0xD,
			kRangefinder1 = 0xE,
			kRangefinder2 = 0xF,
			kRectangle = 0x10,
			kCount = 0x11
		};

		class Data
		{
		public:
			// members
			float         fovMult;       // 00
			std::uint32_t overlay;       // 04
			std::uint32_t isModFormID;   // 08
			NiPoint3      cameraOffset;  // 0C
		};
		static_assert(sizeof(Data) == 0x18);

		// members
		Data                   zoomData;  // 20
		TESImageSpaceModifier* isMod;     // 38
	};
	static_assert(sizeof(BGSZoomData) == 0x40);
}
