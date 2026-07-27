#pragma once

#include "RE/H/hkArray.h"
#include "RE/H/hkRefPtr.h"
#include "RE/H/hkReferencedObject.h"
#include "RE/H/hkStringPtr.h"
#include "RE/H/hknpBodyId.h"
#include "RE/H/hknpConstraintId.h"

namespace RE
{
	class hknpBodyCinfo;
	class hknpConstraintCinfo;
	class hknpMaterial;
	class hknpMotionCinfo;
	class hknpMotionProperties;

	class __declspec(novtable) hknpPhysicsSystemData :
		public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hknpPhysicsSystemData };
		inline static constexpr auto VTABLE{ VTABLE::hknpPhysicsSystemData };

		hknpPhysicsSystemData() { REX::EMPLACE_VTABLE(this); }

		virtual ~hknpPhysicsSystemData();  // 0x00

		// members
		hkArray<hknpMaterial>                       m_materials;          // 0x10
		hkArray<hknpMotionProperties>               m_motionProperties;   // 0x20
		hkArray<hknpMotionCinfo>                    m_motionCinfos;       // 0x30
		hkArray<hknpBodyCinfo>                      m_bodyCinfos;         // 0x40
		hkArray<hknpConstraintCinfo>                m_constraintCinfos;   // 0x50
		hkArray<hkRefPtr<const hkReferencedObject>> m_referencedObjects;  // 0x60
		hkStringPtr                                 m_name;               // 0x70
	};
	static_assert(sizeof(hknpPhysicsSystemData) == 0x78);
}
