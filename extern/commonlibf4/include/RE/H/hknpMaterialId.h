#pragma once

#include "RE/H/hkHandle.h"

namespace RE
{
	class hknpMaterialIdBaseDiscriminant
	{
	public:
	};
	static_assert(std::is_empty_v<hknpMaterialIdBaseDiscriminant>);

	class hknpMaterialId :
		public hkHandle<std::uint16_t, 0xFFFF, hknpMaterialIdBaseDiscriminant>
	{
	public:
	};
	static_assert(sizeof(hknpMaterialId) == 0x02);
}
