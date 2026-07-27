#pragma once

#include "RE/H/hkHandle.h"

namespace RE
{
	struct hknpShapeInstanceIdDiscriminant;

	struct hknpShapeInstanceId :
		public hkHandle<std::int16_t, 0x7FFF, hknpShapeInstanceIdDiscriminant>
	{};
	static_assert(sizeof(hknpShapeInstanceId) == 0x2);
}
