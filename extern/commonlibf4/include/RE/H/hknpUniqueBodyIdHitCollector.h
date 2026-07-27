#pragma once

#include "RE/H/hkRefPtr.h"
#include "RE/H/hknpAllHitsCollector.h"

namespace RE
{
	class hknpBSWorld;

	class __declspec(novtable) hknpUniqueBodyIdHitCollector :
		public hknpAllHitsCollector  // 0x000
	{
	public:
		static constexpr auto RTTI{ RTTI::hknpUniqueBodyIdHitCollector };
		static constexpr auto VTABLE{ VTABLE::hknpUniqueBodyIdHitCollector };

		// members
		hkRefPtr<hknpBSWorld> latentWorld;  // 0x3F0
	};
	static_assert(sizeof(hknpUniqueBodyIdHitCollector) == 0x400);
}
