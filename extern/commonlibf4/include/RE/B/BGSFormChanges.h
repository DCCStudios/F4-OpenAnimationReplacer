#pragma once

#include "RE/B/BGSChangeFlags.h"
#include "RE/B/BGSSaveLoadChangesMap.h"

namespace RE
{
	class BGSUnloadedFormBuffer
	{
	public:
		BGSSaveLoadBuffer Buffer;
	};
	static_assert(sizeof(BGSUnloadedFormBuffer) == 0x8);

	class BGSFormChanges
	{
	public:
		BGSChangeFlags        iChangeFlags;
		BGSUnloadedFormBuffer UnloadedFormBuffer;
	};
	static_assert(sizeof(BGSFormChanges) == 0x10);
}
