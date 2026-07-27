#pragma once

#include "RE/B/BSTHashMap.h"
#include "RE/B/BSSpinLock.h"
#include "RE/B/BGSFormChanges.h"

namespace RE
{
	class BGSSaveLoadChangesMap : public BSTHashMap<unsigned int, BGSFormChanges>
	{
	public:	
		BSReadWriteLock RWLock;
	};
	static_assert(sizeof(BGSSaveLoadChangesMap) == 0x38);
}
