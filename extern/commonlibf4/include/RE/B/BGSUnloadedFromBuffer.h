#pragma once

#include "RE/B/BGSChangeFlags.h"
#include "RE/B/BGSSaveGameBuffer.h"

namespace RE
{
	class BGSUnloadedFormBuffer
	{
	public:
		BGSSaveLoadBuffer Buffer;
	};
	static_assert(sizeof(BGSUnloadedFormBuffer) == 0x8);

}
