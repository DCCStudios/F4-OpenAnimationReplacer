#pragma once

namespace RE
{
	class BGSSaveLoadBuffer
	{
	public:
		// members
		char* buffer;  // 00
	};
	static_assert(sizeof(BGSSaveLoadBuffer) == 0x08);
}
