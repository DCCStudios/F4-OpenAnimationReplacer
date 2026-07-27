#pragma once

namespace RE
{
	enum class SCENE_ACTION_PLAYER_RESPONSE_TYPE : std::uint32_t
	{
		kPositive = 0x0,
		kNegative = 0x1,
		kNeutral = 0x2,
		kQuestion = 0x3,
		kTotal = 0x4,
		kNone = 0x5
	};
}
