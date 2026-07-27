#pragma once

#include "RE/H/hkAabb16.h"
#include "RE/H/hkHalf.h"
#include "RE/H/hkTransform.h"
#include "RE/H/hknpBodyId.h"
#include "RE/H/hknpBodyQualityId.h"
#include "RE/H/hknpCollisionFlags.h"
#include "RE/H/hknpMaterialId.h"
#include "RE/H/hknpMotionId.h"

namespace RE
{
	class hknpShape;

	class hknpBody
	{
	public:
		enum class SpuFlagsEnum : std::int32_t
		{
			kForceNarrowPhasePPU = 0x1
		};

		// members
		hkTransform                         m_transform;                                 // 0x00
		hknpCollisionFlags                  m_flags;                                     // 0x40
		std::uint32_t                       m_collisionFilterInfo;                       // 0x44
		const hknpShape*                    m_shape;                                     // 0x48
		hkAabb16                            m_aabb;                                      // 0x50
		hknpBodyId                          m_id;                                        // 0x60
		hknpBodyId                          m_nextAttachedBodyId;                        // 0x64
		hknpMotionId                        m_motionId;                                  // 0x68
		std::uint32_t                       m_broadPhaseId;                              // 0x6C
		hknpMaterialId                      m_materialId;                                // 0x70
		hknpBodyQualityId                   m_qualityId;                                 // 0x72
		std::uint8_t                        m_timAngle;                                  // 0x73
		std::uint16_t                       m_maxTimDistance;                            // 0x74
		std::uint16_t                       m_maxContactDistance;                        // 0x76
		std::uint32_t                       m_indexIntoActiveListOrDeactivatedIslandId;  // 0x78
		hkHalf                              m_radiusOfComCenteredBoundingSphere;         // 0x7C
		hkFlags<SpuFlagsEnum, std::uint8_t> m_spuFlags;                                  // 0x7E
		std::uint8_t                        m_shapeSizeDiv16;                            // 0x7F
		std::uint16_t                       m_motionToBodyRotation[4];                   // 0x80 - hkPackedUnitVector<4>
		std::uint64_t                       m_userData;                                  // 0x88
	};
	static_assert(sizeof(hknpBody) == 0x90);
}
