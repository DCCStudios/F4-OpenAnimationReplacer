#pragma once

#include "RE/B/BSIntrusiveRefCounted.h"
#include "RE/B/BoneData.h"
#include "RE/N/NiMatrix3.h"

namespace RE
{
	class BaseTreeData :
		public BSIntrusiveRefCounted  // 00
	{
	public:
		// members
		BSTArray<BoneData>  branchBoneData;            // 08
		BSTArray<NiMatrix3> parentWorldBoneRotations;  // 20
	};
	static_assert(sizeof(BaseTreeData) == 0x38);
}
