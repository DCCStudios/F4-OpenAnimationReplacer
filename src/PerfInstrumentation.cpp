#include "PerfInstrumentation.h"

#include <Windows.h>

namespace OARPerf
{
	namespace
	{
		uint64_t QpcFrequency()
		{
			static const uint64_t s_freq = [] {
				LARGE_INTEGER f{};
				QueryPerformanceFrequency(&f);
				return static_cast<uint64_t>(f.QuadPart);
			}();
			return s_freq;
		}

		constexpr double kReportSeconds = 10.0;
		std::atomic<uint64_t> s_frames{ 0 };
		std::chrono::steady_clock::time_point s_lastReport{};
		bool s_haveLastReport = false;
	}

	uint64_t Now() noexcept
	{
		LARGE_INTEGER c{};
		QueryPerformanceCounter(&c);
		return static_cast<uint64_t>(c.QuadPart);
	}

	double TicksToMs(uint64_t a_ticks) noexcept
	{
		return static_cast<double>(a_ticks) * 1000.0 / static_cast<double>(QpcFrequency());
	}

	void FrameTick()
	{
		if (!g_perfEnabled.load(std::memory_order_relaxed)) return;
		const auto now = std::chrono::steady_clock::now();
		if (!s_haveLastReport) {
			s_lastReport = now;
			s_haveLastReport = true;
			return;
		}
		const uint64_t frames = s_frames.fetch_add(1, std::memory_order_relaxed) + 1;
		const double elapsed = std::chrono::duration<double>(now - s_lastReport).count();
		if (elapsed < kReportSeconds) return;

		// Snapshot + reset every category.
		uint64_t ticks[kCount]{};
		uint64_t calls[kCount]{};
		uint64_t maxT[kCount]{};
		for (int i = 0; i < kCount; ++i) {
			ticks[i] = g_ticks[i].exchange(0, std::memory_order_relaxed);
			calls[i] = g_calls[i].exchange(0, std::memory_order_relaxed);
			maxT[i] = g_maxTicks[i].exchange(0, std::memory_order_relaxed);
		}
		s_frames.store(0, std::memory_order_relaxed);
		s_lastReport = now;

		const double fps = frames / elapsed;
		double totalMsPerFrame = 0.0;
		std::string line = fmt::format("[OAR-Perf] {:.1f}s {} frames ({:.1f} fps). Summed CPU across threads, per frame:",
			elapsed, frames, fps);
		for (int i = 0; i < kCount; ++i) {
			const double msPerFrame = TicksToMs(ticks[i]) / static_cast<double>(frames);
			const double callsPerFrame = static_cast<double>(calls[i]) / static_cast<double>(frames);
			const double avgUs = calls[i] ? TicksToMs(ticks[i]) * 1000.0 / static_cast<double>(calls[i]) : 0.0;
			const double maxUs = TicksToMs(maxT[i]) * 1000.0;
			if (!kIsSub[i]) totalMsPerFrame += msPerFrame;
			line += fmt::format("\n    {:<20} {:>8.3f} ms  {:>8.1f} calls  avg {:>7.2f} us  max {:>8.1f} us",
				kNames[i], msPerFrame, callsPerFrame, avgUs, maxUs);
		}
		line += fmt::format("\n    {:<20} {:>8.3f} ms/frame", "TOTAL (top-level)", totalMsPerFrame);
		logger::info("{}", line);
	}
}
