#pragma once

namespace RE
{
	struct alignas(0x8) TeleportLink
	{
	public:
		// members
		TESObjectREFR* door;                  // 00
		NiPoint3       doorTeleportLocation;  // 08
	};
	static_assert(sizeof(TeleportLink) == 0x18);
}
