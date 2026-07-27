#pragma once
#include "RE/G/GrassParam.h"

namespace RE
{
	class TESGrassAreaParam
	{
	public:
		GrassParam grassParam;  // 00
		float      density[9];  // 28
	};
	static_assert(sizeof(TESGrassAreaParam) == 0x50);
}
