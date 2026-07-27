#pragma once

#include "RE/B/BSTArray.h"

namespace RE
{
	class DECAL_CREATION_DATA;

	class BGSDecalGroup
	{
	public:
		// members
		bool                           permanentGroup;  // 00
		bool                           manualSaveLoad;  // 01
		BSTArray<std::uint32_t>        decalGroup;      // 08
		BSTArray<DECAL_CREATION_DATA*> pendingDecals;   // 20
	};
	static_assert(sizeof(BGSDecalGroup) == 0x38);
}
