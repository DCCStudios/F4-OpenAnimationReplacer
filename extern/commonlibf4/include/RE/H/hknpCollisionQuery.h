#pragma once

#include "RE/H/hknpQueryFilterData.h"

namespace RE
{
	class hknpCollisionFilter;

	class hknpCollisionQuery
	{
	public:
		// members
		hkPadSpu<hknpCollisionFilter*> m_filter;         // 0x00
		hknpQueryFilterData            m_filterData;     // 0x08
		std::uint8_t                   m_levelOfDetail;  // 0x18
	};
	static_assert(sizeof(hknpCollisionQuery) == 0x20);
}
