#pragma once

namespace RE
{
	enum class ACTOR_AGGRO_RADIUS : std::uint32_t
	{
		kWarn = 0x0,
		kWarnAndAttack = 0x1,
		kAttack = 0x2,
		kCount = 0x3
	};
}
