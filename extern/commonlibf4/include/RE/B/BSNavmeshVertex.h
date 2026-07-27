#pragma once

#include "RE/N/NiPoint3.h"

namespace RE
{
	class BSNavmeshVertex
	{
	public:
		// members
		NiPoint3 location;  // 0
	};
	static_assert(sizeof(BSNavmeshVertex) == 0xC);
}
