#pragma once

#include "RE/B/BSTArray.h"
#include "RE/B/BSTHashMap.h"

namespace RE
{
	class BGSSaveLoadFormIDMap
	{
	public:	
		BSTHashMap<unsigned int, unsigned int> FormIDToIndexMap;
		BSTHashMap<unsigned int, unsigned int> IndexToFormIDMap;
		unsigned int iCurrentIndex;
	};

	static_assert(sizeof(BGSSaveLoadFormIDMap) == 0x68);
}

