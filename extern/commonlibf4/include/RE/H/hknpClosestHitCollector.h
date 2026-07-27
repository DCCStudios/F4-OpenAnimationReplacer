#pragma once

#include "RE/H/hkArray.h"
#include "RE/H/hknpCollisionQueryCollector.h"
#include "RE/H/hknpCollisionResult.h"

namespace RE
{
	class __declspec(novtable) hknpClosestHitCollector :
		public hknpCollisionQueryCollector  // 0x000
	{
	public:
		static constexpr auto RTTI{ RTTI::hknpClosestHitCollector };
		static constexpr auto VTABLE{ VTABLE::hknpClosestHitCollector };

		// override (hknpCollisionQueryCollector)
		void                       Reset() override;                             // 0x08 [01]
		void                       AddHit(const hknpCollisionResult&) override;  // 0x10 [02]
		bool                       HasHit() const override;                      // 0x18 [03]
		std::int32_t               GetNumHits() const override;                  // 0x20 [04]
		const hknpCollisionResult* GetHits() const override;                     // 0x28 [05]

		// members
		hknpCollisionResult m_result;  // 0x20
		hkBool              m_hasHit;  // 0x80
	};
	static_assert(sizeof(hknpClosestHitCollector) == 0x90);
}
