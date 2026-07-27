#pragma once

#include "RE/B/BSTArray.h"
#include "RE/P/ParentSpaceNode.h"
#include "RE/T/TeleportLink.h"

namespace RE
{
	struct alignas(0x8) TeleportPath
	{
	public:
		BSTArray<ParentSpaceNode, BSTArrayHeapAllocator> spaces;        // 00
		BSTArray<TeleportLink, BSTArrayHeapAllocator>    doors;         // 18
		NiPoint3                                         start;         // 30
		NiPoint3                                         end;           // 3C
		bool                                             worldMapOnly;  // 48
	};
	static_assert(sizeof(TeleportPath) == 0x50);
}
