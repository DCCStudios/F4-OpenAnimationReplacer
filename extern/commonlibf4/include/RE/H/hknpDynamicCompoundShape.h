#pragma once

#include "RE/H/hknpCompoundShape.h"

namespace RE
{
	class hknpDynamicCompoundShapeData;

	class __declspec(novtable) hknpDynamicCompoundShape :
		public hknpCompoundShape
	{
	public:
		static constexpr auto RTTI{ RTTI::hknpDynamicCompoundShape };
		static constexpr auto VTABLE{ VTABLE::hknpDynamicCompoundShape };

		// members
		hknpDynamicCompoundShapeData* m_boundingVolumeData;  // 0xC0
	};
	static_assert(sizeof(hknpDynamicCompoundShape) == 0xD0);
}
