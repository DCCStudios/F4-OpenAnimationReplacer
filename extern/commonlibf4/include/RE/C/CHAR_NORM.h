#pragma once

namespace RE
{
	struct CHAR_NORM
	{
	public:
		// members
		char x;  // 00
		char y;  // 01
		char z;  // 02
	};
	static_assert(sizeof(CHAR_NORM) == 0x03);
}
