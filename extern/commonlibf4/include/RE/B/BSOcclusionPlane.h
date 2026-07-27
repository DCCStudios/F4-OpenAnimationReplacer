#pragma once

#include "RE/B/BSOcclusionShape.h"
#include "RE/N/NiFrustumPlanes.h"

namespace RE
{
	class __declspec(novtable) BSOcclusionPlane :
		public BSOcclusionShape  // 00
	{
	public:
		inline static constexpr auto RTTI{ RTTI::BSOcclusionPlane };
		inline static constexpr auto VTABLE{ VTABLE::BSOcclusionPlane };
		inline static constexpr auto Ni_RTTI{ Ni_RTTI::BSOcclusionPlane };

		~BSOcclusionPlane() override;  // 00

		enum class LINK
		{
			kXNeg = 0x0,
			XPos = 0x1,
			kZNeg = 0x2,
			kZPos = 0x3
		};

		enum class LINKTYPE
		{
			kNone = 0x0,
			kParent = 0x1,
			kChild = 0x2
		};

		// members
		NiPoint2          halfExtents;        // 60
		NiFrustumPlanes   cachedPlanes;       // 68
		NiPoint3          cachedVertices[4];  // D8
		bool              disabledPlanes[4];  // 108
		bool              twoSides;           // 10C
		BSOcclusionPlane* linkedPlanes[4];    // 110
	};
	static_assert(sizeof(BSOcclusionPlane) == 0x130);
}
