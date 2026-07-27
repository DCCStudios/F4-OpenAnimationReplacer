#pragma once

#include "RE/N/NiRefObject.h"

namespace RE
{
	class __declspec(novtable) BGSLOSData :
		public NiRefObject  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::BGSLOSData };
		static constexpr auto VTABLE{ VTABLE::BGSLOSData };

		// members
		std::uint8_t*  losData;              // 10
		std::uint16_t* quadGridOffsetArray;  // 18
		std::uint32_t  losDataSize;          // 20
		std::uint16_t  indexCount;           // 24
	};
	static_assert(sizeof(BGSLOSData) == 0x28);
}
