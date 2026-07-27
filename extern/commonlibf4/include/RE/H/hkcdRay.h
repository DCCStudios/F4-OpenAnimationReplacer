#pragma once

#include "RE/H/hkVector4.h"

namespace RE
{
	class hkcdRay
	{
	public:
		// members
		hkVector4f m_origin;        // 0x00
		hkVector4f m_direction;     // 0x10
		hkVector4f m_invDirection;  // 0x20
	};
	static_assert(sizeof(hkcdRay) == 0x30);
}
