#pragma once

#include "RE/H/hkQuaternion.h"
#include "RE/H/hkVector4.h"

namespace RE
{
	class hkQsTransformf
	{
	public:
		// members
		hkVector4f    m_translation;  // 0x00
		hkQuaternionf m_rotation;     // 0x10
		hkVector4f    m_scale;        // 0x20
	};
	static_assert(sizeof(hkQsTransformf) == 0x30);

	using hkQsTransform = hkQsTransformf;
}
