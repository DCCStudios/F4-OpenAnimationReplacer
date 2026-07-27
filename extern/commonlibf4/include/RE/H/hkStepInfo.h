#pragma once

#include "RE/H/hkPadSpu.h"

namespace RE
{
	class hkStepInfo
	{
	public:
		// members
		hkPadSpu<float> m_startTime;     // 0x00
		hkPadSpu<float> m_endTime;       // 0x04
		hkPadSpu<float> m_deltaTime;     // 0x08
		hkPadSpu<float> m_invDeltaTime;  // 0x0C
	};
	static_assert(sizeof(hkStepInfo) == 0x10);
}
