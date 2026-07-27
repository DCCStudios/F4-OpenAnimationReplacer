#pragma once

namespace RE
{
	class BGSSaveLoadFormInfo
	{
	public:	
		std::uint8_t cData;
	};

	static_assert(sizeof(BGSSaveLoadFormInfo) == 0x1);
}
