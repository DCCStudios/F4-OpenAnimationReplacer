#pragma once

#include "RE/B/BSTHashMap.h"
#include "RE/B/BGSLoadGameSubBuffer.h"


namespace RE
{
	
	class BGSSaveLoadQueuedSubBufferMap
	{
	public:	
		BSTHashMap<TESForm*, BGSLoadGameSubBuffer> QueuedSubBuffers[3];
	};
	static_assert(sizeof(BGSSaveLoadQueuedSubBufferMap) == 0x90);
}

