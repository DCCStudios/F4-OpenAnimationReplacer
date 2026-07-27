#pragma once

#include "RE/B/BSOcclusionShape.h"
#include "RE/N/NiFrustumPlanes.h"
#include "RE/N/NiPoint3.h"

namespace RE
{
	class __declspec(novtable) BSOcclusionBox :
		public BSOcclusionShape  // 00
	{
	public:
		inline static constexpr auto RTTI{ RTTI::BSOcclusionBox };
		inline static constexpr auto VTABLE{ VTABLE::BSOcclusionBox };
		inline static constexpr auto Ni_RTTI{ Ni_RTTI::BSOcclusionBox };

		~BSOcclusionBox() override;  // 00

		// members
		NiPoint3A          halfExtents;             // 60
		NiFrustumPlanes    cachedPlanes[2];         // 70
		NiPoint3A          cacvesVertices[8];       // 150
		BSMultiBoundShape* mbShape;                 // 1D0
		bool               disablesPlanes[12];      // 1D8
		std::int32_t       secondaryFacePlanes[6];  // 1E4
		std::uint32_t      hullIndex[8];            // 1FC
		std::uint32_t      hullLength;              // 21C
	};
	static_assert(sizeof(BSOcclusionBox) == 0x220);
}
