#pragma once

namespace RE
{
	class AITimer
	{
	public:
		static float fTimer()
		{
			static REL::Relocation<float*> singleton{ ID::AITimer::fTimer };
			return *singleton;
		}

		// members
		float startTime;   // 00
		float targetTime;  // 04
	};
	static_assert(sizeof(AITimer) == 0x8);
}
