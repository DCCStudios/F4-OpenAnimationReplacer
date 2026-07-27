#pragma once

#include "RE/H/hkQuaternion.h"
#include "RE/H/hkRefPtr.h"
#include "RE/H/hkStringPtr.h"
#include "RE/H/hkVector4.h"
#include "RE/H/hknpBody.h"

namespace RE
{
	class hkLocalFrame;

	class hknpBodyCinfo
	{
	public:
		hknpBodyCinfo()
		{
			using func_t = void (*)(hknpBodyCinfo*);
			static REL::Relocation<func_t> func{ ID::hknpBodyCinfo::ctor };
			func(this);
		}

		// members
		const hknpShape*                              m_shape;                       // 0x00
		hknpBodyId                                    m_reservedBodyId;              // 0x08
		hknpMotionId                                  m_motionId;                    // 0x0C
		hknpBodyQualityId                             m_qualityId;                   // 0x10
		hknpMaterialId                                m_materialId;                  // 0x12
		std::uint32_t                                 m_collisionFilterInfo;         // 0x14
		hknpCollisionFlags                            m_flags;                       // 0x18
		float                                         m_collisionLookAheadDistance;  // 0x1C
		hkStringPtr                                   m_name;                        // 0x20
		std::uint64_t                                 m_userData;                    // 0x28
		hkVector4f                                    m_position;                    // 0x30
		hkQuaternionf                                 m_orientation;                 // 0x40
		hkFlags<hknpBody::SpuFlagsEnum, std::uint8_t> m_spuFlags;                    // 0x50
		hkRefPtr<hkLocalFrame>                        m_localFrame;                  // 0x58
	};
	static_assert(sizeof(hknpBodyCinfo) == 0x60);
}
