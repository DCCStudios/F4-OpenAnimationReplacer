#pragma once

namespace RE
{
	class CountdownTimer
	{
	public:
		// members
		std::uint64_t startTime;  // 00
		std::uint64_t endTime;    // 08
		bool          isActive;   // 10
	};
	static_assert(sizeof(CountdownTimer) == 0x18);
}
