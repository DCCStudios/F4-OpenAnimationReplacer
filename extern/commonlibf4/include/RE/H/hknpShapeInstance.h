#pragma once

#include "RE/H/hkRefPtr.h"
#include "RE/H/hkTransform.h"

namespace RE
{
	class hknpShape;

	class hknpShapeInstance
	{
	public:
		// members
		hkTransformf              m_transform;       // 0x00
		hkVector4f                m_scale;           // 0x40
		hkRefPtr<const hknpShape> m_shape;           // 0x50
		std::uint16_t             m_shapeTag;        // 0x58
		std::uint16_t             m_destructionTag;  // 0x5A
		std::byte                 m_padding[30];     // 0x5C
		std::byte                 m_pad7A[6];        // 0x7A
	};
	static_assert(sizeof(hknpShapeInstance) == 0x80);
}
