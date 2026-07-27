#pragma once

#include "RE/B/BGSSaveLoadBuffer.h"

namespace RE
{
	class BGSLoadGameSubBuffer
	{
	public:	
		BGSSaveLoadBuffer Buffer;
	};

	static_assert(sizeof(BGSLoadGameSubBuffer) == 0x8);
}
