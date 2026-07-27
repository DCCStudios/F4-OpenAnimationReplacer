#pragma once

namespace RE
{
	class hclVectorBuffer
	{
	public:
		// members
		void*         m_bufferStart;  // 0x00
		std::uint32_t m_numElements;  // 0x08
		std::uint8_t  m_stride;       // 0x0C
		std::uint32_t m_flags;        // 0x10
	};
	static_assert(sizeof(hclVectorBuffer) == 0x18);
}
