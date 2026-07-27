#pragma once

#include "RE/H/hkBool.h"

namespace RE
{
	class hclTriangleBuffer
	{
	public:
		// members
		const void*   m_bufferStart;       // 0x00
		std::uint32_t m_numElements;       // 0x08
		std::uint8_t  m_stride;            // 0x0C
		hkBool        m_use16BitsIndices;  // 0x0D
	};
	static_assert(sizeof(hclTriangleBuffer) == 0x10);
}
