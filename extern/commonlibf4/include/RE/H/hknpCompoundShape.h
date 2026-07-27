#pragma once

#include "RE/H/hkAabb.h"
#include "RE/H/hkBool.h"
#include "RE/H/hkFreeListArray.h"
#include "RE/H/hknpCompositeShape.h"
#include "RE/H/hknpShapeInstance.h"
#include "RE/H/hknpShapeInstanceId.h"

namespace RE
{
	class __declspec(novtable) hknpCompoundShape :
		public hknpCompositeShape
	{
	public:
		static constexpr auto RTTI{ RTTI::hknpCompoundShape };
		static constexpr auto VTABLE{ VTABLE::hknpCompoundShape };

		// add
		virtual bool UpdateAabb() = 0;                                                    // 1E
		virtual void GetInstanceAabb(hknpShapeInstanceId a_id, hkAabb& a_out) const = 0;  // 1F

		// members
		hkFreeListArray<hknpShapeInstance, hknpShapeInstanceId, 8, hknpShapeInstance> instances;              // 0x60
		hkAabb                                                                        aabb;                   // 0x80
		hkBool                                                                        isMutable;              // 0xA0
		std::byte                                                                     mutationSignals[0x10];  // 0xA1 - hknpShapeSignals
	};
	static_assert(sizeof(hknpCompoundShape) == 0xC0);
}
