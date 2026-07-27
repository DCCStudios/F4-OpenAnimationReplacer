#pragma once

namespace RE
{
	class CubeMapSide
	{
	public:
		// members
		std::uint32_t side;        // 00
		float         renderTime;  // 04
	};
	static_assert(sizeof(CubeMapSide) == 0x08);
}
