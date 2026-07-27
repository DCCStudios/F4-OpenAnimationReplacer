#pragma once

#include "RE/H/hknpShape.h"
#include "RE/H/hknpSparseCompactMap.h"

namespace RE
{
	class __declspec(novtable) hknpCompositeShape :
		public hknpShape
	{
	public:
		static constexpr auto RTTI{ RTTI::hknpCompositeShape };
		static constexpr auto VTABLE{ VTABLE::hknpCompositeShape };

		// members
		hknpSparseCompactMap<std::uint16_t> edgeWeldingMap;     // 0x30
		std::uint32_t                       shapeTagCodecInfo;  // 0x58
	};
	static_assert(sizeof(hknpCompositeShape) == 0x60);
}
