#pragma once

#include "RE/B/BSMultiBoundNode.h"
#include "RE/B/BSPortal.h"
#include "RE/N/NiTPointerList.h"

namespace RE
{
	class BSLight;

	class __declspec(novtable) BSMultiBoundRoom :
		public BSMultiBoundNode  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::BSMultiBoundRoom };
		static constexpr auto VTABLE{ VTABLE::BSMultiBoundRoom };
		static constexpr auto Ni_RTTI{ Ni_RTTI::BSMultiBoundRoom };

		// members
		NiTPointerList<BSPortal*>                 portalList;            // 150
		NiTPointerList<BSOcclusionShape*>         occluderList;          // 168
		NiTPointerList<NiPointer<BSMultiBound>*>  joinedMultiBoundList;  // 180
		BSTArray<BSLight*, BSTArrayHeapAllocator> lights;                // 198
	};
	static_assert(sizeof(BSMultiBoundRoom) == 0x1B0);
}
