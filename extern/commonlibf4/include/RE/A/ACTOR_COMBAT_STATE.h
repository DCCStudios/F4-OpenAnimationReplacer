#pragma once

namespace RE
{
	enum class ACTOR_COMBAT_STATE : std::int32_t
	{
		kNone = 0x0,
		kCombat = 0x1,
		kSearching = 0x2
	};
}
