#pragma once

#include "RE/B/BSCoreUtils.h"
#include "RE/N/NiCamera.h"
#include "RE/N/NiFrustumPlanes.h"

namespace RE
{
	struct BSCompoundFrustum
	{
		enum class Operator
		{
			kNoOp = 0x0,
			kEOL = 0x1,
			kEOLPass = 0x2,
			kEOLFail = 0x3,
			kGroupIntersection = 0x4,
			kGroupUnion = 0x5,
			kGroupEnd = 0x6,
			kPortal = 0x7,
			kOcclusion = 0x8,
			kOcclusionBox = 0x9
		};

		struct FunctionOp
		{
			std::int32_t op;  // 00
			std::int32_t jp;  // 04
			std::int32_t jf;  // 08
		};
		static_assert(sizeof(FunctionOp) == 0x0C);

		// members
		BSTArray<NiFrustumPlanes, BSTArrayHeapAllocator> planes;             // 00
		BSTArray<FunctionOp, BSTArrayHeapAllocator>      functionOperators;  // 18
		NiFrustumPlanes                                  viewFrustum;        // 30
		NiPoint3                                         viewPosition;       // A0
		const NiCamera*                                  camera;             // B0
		const BSCoreUtils::BSCPUFeatures* const          CPUFeatures;        // B8
		std::uint32_t                                    freePlane;          // C0
		std::uint32_t                                    freeOp;             // C4
		std::uint32_t                                    firstOp;            // C8
		bool                                             skipViewFrustum;    // CC
		bool                                             preThreaded;        // CD
	};
	static_assert(sizeof(BSCompoundFrustum) == 0xD0);
}
