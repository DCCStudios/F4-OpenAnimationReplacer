#pragma once

namespace RE
{
	class ScrapHeap;

	class BGSSaveLoadScrapBuffer
	{
	public:
		// members
		char*         buffer;     // 00
		ScrapHeap*    scrapHeap;  // 08
		std::uint32_t size;       // 10
	};
	static_assert(sizeof(BGSSaveLoadScrapBuffer) == 0x18);
}
