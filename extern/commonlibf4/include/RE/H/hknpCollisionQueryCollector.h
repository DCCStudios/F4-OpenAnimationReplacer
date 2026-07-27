#pragma once

#include "RE/H/hkBaseObject.h"
#include "RE/H/hkSimdFloat32.h"

namespace RE
{
	class hknpCollisionResult;

	class __declspec(novtable) hknpCollisionQueryCollector :
		public hkBaseObject  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::hknpCollisionQueryCollector };
		static constexpr auto VTABLE{ VTABLE::hknpCollisionQueryCollector };

		// add
		virtual void                       Reset() = 0;                                   // 0x08 [01]
		virtual void                       AddHit(const hknpCollisionResult& a_hit) = 0;  // 0x10 [02]
		virtual bool                       HasHit() const = 0;                            // 0x18 [03]
		virtual std::int32_t               GetNumHits() const = 0;                        // 0x20 [04]
		virtual const hknpCollisionResult* GetHits() const = 0;                           // 0x28 [05]

		// members
		std::int32_t  m_hints;              // 0x08
		hkSimdFloat32 m_earlyOutThreshold;  // 0x10
	};
	static_assert(sizeof(hknpCollisionQueryCollector) == 0x20);
}
