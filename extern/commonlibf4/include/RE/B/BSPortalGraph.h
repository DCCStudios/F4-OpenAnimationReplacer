#pragma once

#include "RE/B/BSOcclusionShape.h"
#include "RE/B/BSPortal.h"
#include "RE/N/NiTPointerList.h"

namespace RE
{
	class __declspec(novtable) alignas(0x8) BSPortalGraph :
		public NiRefObject  // 00
	{
	public:
		inline static constexpr auto RTTI{ RTTI::BSPortalGraph };
		inline static constexpr auto VTABLE{ VTABLE::BSPortalGraph };

		// members
		NiTPointerList<BSOcclusionShape*>                            occluderList;             // 10
		NiTPointerList<BSPortal*>                                    portalList;               // 28
		BSTArray<NiPointer<BSMultiBoundRoom>, BSTArrayHeapAllocator> multiBoundRoomList;       // 40
		BSTArray<NiPointer<NiAVObject>, BSTArrayHeapAllocator>       alwayRenderChildren;      // 58
		NiPointer<NiNode>                                            portalNodeRoot;           // 70
		BSTArray<NiPointer<NiAVObject>, BSTArrayHeapAllocator>       attachAlwaysRenderQueue;  // 78
		BSTArray<NiPointer<NiAVObject>, BSTArrayHeapAllocator>       detachAlwaysRenderQueue;  // 90
		BSTArray<NiPointer<NiNode>, BSTArrayHeapAllocator>           unboundNodes;             // A8
		std::uint32_t                                                worldID;                  // C0
	};
	static_assert(sizeof(BSPortalGraph) == 0xC8);
}
