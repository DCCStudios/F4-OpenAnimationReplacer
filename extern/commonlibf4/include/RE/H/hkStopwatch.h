#pragma once

namespace RE
{
	class hkStopwatch
	{
	public:
		hkStopwatch() = default;

		// members
		std::int64_t m_ticksAtStart{ 0 };     // 0x00
		std::int64_t m_ticksTotal{ 0 };       // 0x08
		std::int64_t m_ticksAtSplit{ 0 };     // 0x10
		std::int64_t m_splitTotal{ 0 };       // 0x18
		hkBool       m_runningFlag{ false };  // 0x20
		std::int32_t m_numTimings{ 0 };       // 0x24
		const char*  m_name{ nullptr };       // 0x28
	};
	static_assert(sizeof(hkStopwatch) == 0x30);
}
