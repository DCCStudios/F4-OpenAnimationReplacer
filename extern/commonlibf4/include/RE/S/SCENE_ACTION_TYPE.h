#pragma once

namespace RE
{
	enum class SCENE_ACTION_TYPE : std::uint32_t
	{
		kDialogue = 0x0,
		kPackage = 0x1,
		kTimer = 0x2,
		kPlayerDialogue = 0x3,
		kStartScene = 0x4,
		kNPCResponse = 0x5,
		kRadio = 0x6,
	};
}
