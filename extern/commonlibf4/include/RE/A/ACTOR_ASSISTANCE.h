#pragma once

namespace RE
{
	enum class ACTOR_ASSISTANCE : std::uint32_t
	{
		kHelpsNobody = 0x0,
		kHelpsAllies = 0x1,
		kHelpsFriends = 0x2,
		kCount = 0x3
	};
}
