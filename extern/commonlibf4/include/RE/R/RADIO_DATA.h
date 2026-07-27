#pragma once

namespace RE
{
	class RADIO_DATA
	{
	public:
		// members
		float radioStation;         // 00
		float minWeakSignalRange;   // 04
		float maxWeakSignalRange;   // 08
		bool  ignoreDistanceChekc;  // 0C
	};
	static_assert(sizeof(RADIO_DATA) == 0x10);
}
