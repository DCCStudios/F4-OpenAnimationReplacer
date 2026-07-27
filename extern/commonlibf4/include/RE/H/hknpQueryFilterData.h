#pragma once

#include "RE/H/hkPadSpu.h"
#include "RE/H/hknpMaterialId.h"

namespace RE
{
	class hknpQueryFilterData
	{
	public:
		// members
		hknpMaterialId          m_materialId;           // 0x00
		hkPadSpu<std::uint32_t> m_collisionFilterInfo;  // 0x04
		hkPadSpu<std::uint64_t> m_userData;             // 0x08
	};
	static_assert(sizeof(hknpQueryFilterData) == 0x10);
}
