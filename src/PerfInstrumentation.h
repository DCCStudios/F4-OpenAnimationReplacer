#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

// Lightweight frame-time instrumentation for OAR's own hooks.
//
// Each category accumulates CPU time (QueryPerformanceCounter ticks) and call
// counts across ALL threads; Havok updates clips from several job threads, so
// the per-frame figures are summed CPU time, not wall-clock. Every
// kReportSeconds the player-update tick logs one "[OAR-Perf]" line with
// per-frame averages and resets. Overhead per scope is two QPC reads (~20 ns
// each) plus two relaxed atomic adds; when g_perfEnabled is false a scope
// costs one relaxed load.
//
// Categories marked "sub" run INSIDE another measured scope and are reported
// for attribution only; TOTAL sums the top-level categories.
namespace OARPerf
{
	enum Category : uint8_t
	{
		kUpdate = 0,          // hkbClipGenerator_Update, OAR work only (after the engine call)
		kUpdateNoMatch,       // sub: Update calls that ended at "no registered replacement"
		kCacheGetOrBuild,     // sub: AnimationCache::GetOrBuildRuntimeAnim (inside Update/Activate)
		kGenerate,            // hkbClipGenerator_Generate, OAR work only (after the engine call)
		kTrackFilter,         // sub: the track-filter block inside Generate
		kActivate,            // hkbClipGenerator_Activate, whole hook incl. the engine's Activate
		kEventFeed,           // animation-event sink (per event, all actors)
		kHealSkeleton,        // HealSkeletonRootNaN (both call sites)
		kPollPlayerGraph,     // PollPlayerGraphClips (once per frame)
		kCount
	};

	inline constexpr const char* kNames[kCount] = {
		"Update", "  Update.noMatch", "  Cache.getOrBuild", "Generate", "  TrackFilter",
		"Activate", "EventFeed", "HealSkeleton", "PollPlayerGraph"
	};
	inline constexpr bool kIsSub[kCount] = {
		false, true, true, false, true, false, false, false, false
	};

	inline std::atomic<bool> g_perfEnabled{ true };
	inline std::array<std::atomic<uint64_t>, kCount> g_ticks{};
	inline std::array<std::atomic<uint64_t>, kCount> g_calls{};
	inline std::array<std::atomic<uint64_t>, kCount> g_maxTicks{};

	uint64_t Now() noexcept;   // QPC ticks
	double TicksToMs(uint64_t a_ticks) noexcept;

	inline void Add(Category a_cat, uint64_t a_ticks) noexcept
	{
		g_ticks[a_cat].fetch_add(a_ticks, std::memory_order_relaxed);
		g_calls[a_cat].fetch_add(1, std::memory_order_relaxed);
		uint64_t prev = g_maxTicks[a_cat].load(std::memory_order_relaxed);
		while (a_ticks > prev &&
			!g_maxTicks[a_cat].compare_exchange_weak(prev, a_ticks, std::memory_order_relaxed)) {}
	}

	struct Scope
	{
		explicit Scope(Category a_cat) noexcept
			: cat(a_cat), start(g_perfEnabled.load(std::memory_order_relaxed) ? Now() : 0) {}
		~Scope() { if (start) Add(cat, Now() - start); }
		// Attribute the time elapsed so far to a second (sub) category as well.
		void Split(Category a_sub) const noexcept { if (start) Add(a_sub, Now() - start); }
		Category cat;
		uint64_t start;
	};

	// Called once per frame (player update tail). Logs and resets every
	// kReportSeconds.
	void FrameTick();
}

#define OAR_PERF_SCOPE(cat) ::OARPerf::Scope _oarPerfScope##__LINE__{ ::OARPerf::cat }
#define OAR_PERF_SCOPE_NAMED(var, cat) ::OARPerf::Scope var{ ::OARPerf::cat }
