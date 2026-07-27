#pragma once

#include "RE/B/BSTHashMap.h"
#include <RE/C/CellAttachDetachEventSource.h>

namespace RE
{
	using BGSCellNumericIDArrayMap = RE::BSTHashMap<unsigned int, RE::BSTArray<unsigned int, RE::BSTArrayHeapAllocator>*>;
	
	class BGSSaveLoadReferencesMap
	{
	public:	
		BSTHashMap<unsigned int, unsigned int>				MovedReferencesMap;
		BGSCellNumericIDArrayMap                            InteriorReferencesMap;
		BSTHashMap<unsigned int, BGSCellNumericIDArrayMap*>	WorldspaceReferencesMap;
		BSSpinLock                                          Lock;
	};

	static_assert(sizeof(BGSSaveLoadReferencesMap) == 0x98);
}
