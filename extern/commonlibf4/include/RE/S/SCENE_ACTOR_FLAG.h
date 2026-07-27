#pragma once

namespace RE
{
	enum class SCENE_ACTOR_FLAG : std::uint32_t
	{
		kNoPlayerActivation = 0x0,
		kOption = 0x1,
		kRunOnlyScenePackages = 0x2,
		kNoCommandState = 0x3,
		kCount = 0x4
	};
}
