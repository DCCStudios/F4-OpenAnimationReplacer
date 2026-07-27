#pragma once

#include "RE/H/hkMatrix3.h"

namespace RE
{
	class hkRotationf :
		public hkMatrix3f
	{
	public:
		constexpr hkRotationf() noexcept = default;

		using hkMatrix3f::hkMatrix3f;
	};
	static_assert(sizeof(hkRotationf) == 0x30);
}
