#pragma once

namespace RE
{
	class BSTimer
	{
	public:
		[[nodiscard]] static BSTimer* GetSingleton()
		{
			static REL::Relocation<BSTimer*> singleton{ ID::BSTimer::Singleton };
			return singleton.get();
		}

		[[nodiscard]] static float QGlobalTimeMultiplier()
		{
			static REL::Relocation<float*> value{ ID::BSTimer::QGlobalTimeMultiplier };
			return *value;
		}

		[[nodiscard]] static float QGlobalTimeMultiplierTarget()
		{
			static REL::Relocation<float*> value{ ID::BSTimer::QGlobalTimeMultiplierTarget };
			return *value;
		}

		void SetGlobalTimeMultiplier(float a_mult, bool a_now)
		{
			using func_t = decltype(&BSTimer::SetGlobalTimeMultiplier);
			static REL::Relocation<func_t> func{ ID::BSTimer::SetGlobalTimeMultiplier };
			return func(this, a_mult, a_now);
		}

		// members
		std::int64_t  highPrecisionInitTime;          // 00
		float         clamp;                          // 08
		float         clampRemainder;                 // 0C
		float         delta;                          // 10
		float         realTimeDelta;                  // 14
		std::uint64_t lastTime;                       // 18
		std::byte     unk20[0x10];                    // 20
		std::uint64_t firstTime;                      // 30
		std::uint64_t disabledLastTime;               // 38
		std::uint64_t disabledFirstTime;              // 40
		std::uint32_t disableCounter;                 // 48
		bool          useGlobalTimeMultiplierTarget;  // 4C
	};
	static_assert(sizeof(BSTimer) == 0x50);
}
