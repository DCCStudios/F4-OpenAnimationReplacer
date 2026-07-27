#pragma once

#include "RE/B/BSCompoundFrustum.h"
#include "RE/B/BSMultiBoundRoom.h"
#include "RE/B/BSPortalGraph.h"

namespace RE
{
	class __declspec(novtable) alignas(0x8) BSPortalGraphEntry :
		public NiRefObject  // 00
	{
	public:
		inline static constexpr auto RTTI{ RTTI::BSPortalGraphEntry };
		inline static constexpr auto VTABLE{ VTABLE::BSPortalGraphEntry };

		enum class PGE_ROOTVIS
		{
			kFrustrum = 0x0,
			kSphere = 0x1,
			kHemiSphere = 0x2
		};

		// members
		BSPortalGraph*                                               portalGraph;              // 10
		BSTArray<NiPointer<BSMultiBoundRoom>, BSTArrayHeapAllocator> multiboundRoomAccumList;  // 18
		NiPointer<BSMultiBoundRoom>                                  multiBoundRoomRoot;       // 30
		BSTHashMap<void const*, BSCompoundFrustum*>                  visibilityMap;            // 38
		BSCompoundFrustum                                            compoundFrustum;          // 68
		bool                                                         visibleUnboundSpace;      // 138
		bool                                                         enableAccumulation;       // 139
		PGE_ROOTVIS                                                  rootVisibilityType;       // 13C
		std::uint32_t                                                worldID;                  // 140
	};
	static_assert(sizeof(BSPortalGraphEntry) == 0x148);
}
