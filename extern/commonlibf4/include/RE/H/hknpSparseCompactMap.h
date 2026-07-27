#pragma once

#include "RE/H/hkArray.h"

namespace RE
{
	template <class T>
	class hknpSparseCompactMap
	{
	public:
	private:
		std::uint32_t m_secondaryKeyMask;       // 0x00
		std::uint32_t m_sencondaryKeyBits;      // 0x04
		hkArray<T>    m_primaryKeyToIndex;      // 0x08
		hkArray<T>    m_valueAndSecondaryKeys;  // 0x18
	};
	static_assert(sizeof(hknpSparseCompactMap<std::uint16_t>) == 0x28);
}
