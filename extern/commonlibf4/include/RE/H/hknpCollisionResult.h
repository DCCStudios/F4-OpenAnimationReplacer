#pragma once

#include "RE/C/CFilter.h"
#include "RE/H/hkPadSpu.h"
#include "RE/H/hkVector4.h"
#include "RE/H/hknpBodyId.h"
#include "RE/H/hknpCollisionQueryType.h"
#include "RE/H/hknpMaterialId.h"

namespace RE
{
	class hknpCollisionResult
	{
	public:
		class BodyInfo
		{
		public:
			// members
			hknpBodyId              bodyId;                    // 0x00
			hknpMaterialId          shapeMaterialId;           // 0x04
			hkPadSpu<std::uint32_t> shapeKey;                  // 0x08
			hkPadSpu<CFilter>       shapeCollisionFilterInfo;  // 0x0C
			hkPadSpu<std::uint64_t> shapeUserData;             // 0x10
		};
		static_assert(sizeof(BodyInfo) == 0x18);

		// members
		hkVector4f                   position;       // 0x00
		hkVector4f                   normal;         // 0x10
		hkPadSpu<float>              fraction;       // 0x20
		BodyInfo                     queryBodyInfo;  // 0x28
		BodyInfo                     hitBodyInfo;    // 0x40
		hknpCollisionQueryType::Enum queryType;      // 0x58
		hkPadSpu<std::uint32_t>      hitResult;      // 0x5C
	};
	static_assert(sizeof(hknpCollisionResult) == 0x60);
}
