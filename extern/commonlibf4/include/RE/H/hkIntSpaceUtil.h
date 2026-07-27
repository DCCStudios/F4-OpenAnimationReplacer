#pragma once

#include "RE/H/hkAabb.h"
#include "RE/H/hkVector4.h"

namespace RE
{
	class hkIntSpaceUtil
	{
	public:
		// members
		hkVector4f m_bitOffsetLow;   // 0x00
		hkVector4f m_bitOffsetHigh;  // 0x10
		hkVector4f m_bitScale;       // 0x20
		hkVector4f m_bitScaleInv;    // 0x30
		hkAabb     m_aabb;           // 0x40
	};
	static_assert(sizeof(hkIntSpaceUtil) == 0x60);
}
