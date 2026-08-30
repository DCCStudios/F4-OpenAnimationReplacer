#pragma once

#include <atomic>
#include <chrono>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

class SubMod;

struct ActiveReplacementEntry
{
	std::string clipSuffix;
	std::string subModName;
	std::string replacementPath;
	// Full resolved on-disk path of the original animation (from the subgraph
	// swap-array resolution), when known. Display-only.
	std::string fullPath;
	std::string actorName;
	uint32_t actorFormID{ 0 };
	bool conditionsPassed{ false };
	const SubMod* subMod{ nullptr }; // For live re-evaluation of conditions in the UI
	// Names of other submods whose conditions ALSO currently pass for this clip
	// but lost to the winner on priority (only populated while the window is open;
	// see the live-view scan in the clip hook). Display-only.
	std::vector<std::string> overriddenSubMods;
};

class ActiveReplacementTracker
{
public:
	static ActiveReplacementTracker* GetSingleton()
	{
		static ActiveReplacementTracker singleton;
		return &singleton;
	}

	// Key: (actorFormID, clipSuffix) — prevents orphaned pointer entries
	struct CompositeKey
	{
		uint32_t actorFormID;
		std::string clipSuffix;

		bool operator==(const CompositeKey& o) const
		{
			return actorFormID == o.actorFormID && clipSuffix == o.clipSuffix;
		}
	};

	struct CompositeKeyHash
	{
		size_t operator()(const CompositeKey& k) const
		{
			size_t h1 = std::hash<uint32_t>{}(k.actorFormID);
			size_t h2 = std::hash<std::string>{}(k.clipSuffix);
			return h1 ^ (h2 << 1);
		}
	};

	struct TimedEntry
	{
		ActiveReplacementEntry entry;
		std::chrono::steady_clock::time_point lastTouched;
	};

	void Update(uint32_t a_actorFormID, const std::string& a_clipSuffix, const ActiveReplacementEntry& a_entry)
	{
		CompositeKey key{ a_actorFormID, a_clipSuffix };
		const auto now = std::chrono::steady_clock::now();
		// While the Active Replacements window is open it flags live view, and we
		// stop throttling so lastTouched is bumped EVERY frame — that per-frame
		// freshness is what lets PurgeStale drop a clip within a couple of frames
		// of it stopping (see kLivePurgeMs).
		const bool liveView = IsLiveViewActive();
		{
			std::shared_lock lock(m_mutex);
			auto it = m_active.find(key);
			if (it != m_active.end()) {
				const auto& current = it->second.entry;
				const bool unchanged =
					current.clipSuffix == a_entry.clipSuffix &&
					current.subModName == a_entry.subModName &&
					current.replacementPath == a_entry.replacementPath &&
					current.fullPath == a_entry.fullPath &&
					current.actorName == a_entry.actorName &&
					current.actorFormID == a_entry.actorFormID &&
					current.conditionsPassed == a_entry.conditionsPassed &&
					current.subMod == a_entry.subMod &&
					current.overriddenSubMods == a_entry.overriddenSubMods;
				if (unchanged && !liveView && now - it->second.lastTouched < kRefreshInterval) {
					return;
				}
			}
		}

		std::unique_lock lock(m_mutex);
		auto& timed = m_active[key];
		const auto& current = timed.entry;
		const bool unchanged =
			current.clipSuffix == a_entry.clipSuffix &&
			current.subModName == a_entry.subModName &&
			current.replacementPath == a_entry.replacementPath &&
			current.fullPath == a_entry.fullPath &&
			current.actorName == a_entry.actorName &&
			current.actorFormID == a_entry.actorFormID &&
			current.conditionsPassed == a_entry.conditionsPassed &&
			current.subMod == a_entry.subMod;
		if (!unchanged) {
			timed.entry = a_entry;
		}
		timed.lastTouched = now;
	}

	void Remove(uint32_t a_actorFormID, const std::string& a_clipSuffix)
	{
		CompositeKey key{ a_actorFormID, a_clipSuffix };
		std::unique_lock lock(m_mutex);
		m_active.erase(key);
	}

	void Clear()
	{
		std::unique_lock lock(m_mutex);
		m_active.clear();
	}

	std::vector<ActiveReplacementEntry> GetSnapshot() const
	{
		std::shared_lock lock(m_mutex);
		std::vector<ActiveReplacementEntry> result;
		result.reserve(m_active.size());
		for (auto& [_, timed] : m_active) {
			result.push_back(timed.entry);
		}
		return result;
	}

	size_t GetCount() const
	{
		std::shared_lock lock(m_mutex);
		return m_active.size();
	}

	void PurgeStale()
	{
		const auto now = std::chrono::steady_clock::now();
		// Live view (window open): drop a clip within a couple of frames of it
		// stopping. Safe only because Update() touches active clips every frame in
		// live mode, so a still-playing clip is never older than one frame here.
		// Idle (window closed): keep the generous window so a reopened window shows
		// recent history and the animation thread pays no extra locking.
		const long long thresholdMs = IsLiveViewActive() ? kLivePurgeMs : kIdlePurgeMs;
		std::unique_lock lock(m_mutex);
		for (auto it = m_active.begin(); it != m_active.end();) {
			auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastTouched).count();
			if (age > thresholdMs) {
				it = m_active.erase(it);
			} else {
				++it;
			}
		}
	}

	// Called by the Active Replacements window every frame it is drawn. Enables
	// per-frame refresh: Update() stops throttling and PurgeStale() switches to a
	// tight eviction window. On the transition into live view every existing entry
	// is touched once, so the tight purge cannot drop still-playing clips before
	// the per-frame Update() cadence has had a frame to refresh them.
	void SetLiveViewActive()
	{
		const auto now = std::chrono::steady_clock::now();
		const bool wasActive = IsLiveViewActive();
		m_liveViewUntilNs.store(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				(now + std::chrono::milliseconds(kLiveViewLingerMs)).time_since_epoch())
				.count(),
			std::memory_order_relaxed);
		if (!wasActive) {
			std::unique_lock lock(m_mutex);
			for (auto& [_, timed] : m_active) {
				timed.lastTouched = now;
			}
		}
	}

	bool IsLiveViewActive() const
	{
		const auto until = m_liveViewUntilNs.load(std::memory_order_relaxed);
		if (until == 0) return false;
		const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
			.count();
		return until > nowNs;
	}

private:
	// The tracker feeds diagnostics/UI, not animation state. Refreshing an
	// unchanged entry at this cadence keeps PurgeStale semantics while avoiding
	// an exclusive lock and repeated string assignment on every clip update.
	// Bypassed while the window is in live view (see Update / SetLiveViewActive).
	static constexpr auto kRefreshInterval = std::chrono::seconds(1);
	// Eviction age used while the window is open (live view) vs closed (idle).
	static constexpr long long kLivePurgeMs = 120;
	static constexpr long long kIdlePurgeMs = 30000;
	// How long live view stays armed after the last SetLiveViewActive() call, so
	// a one-frame gap in drawing doesn't drop back to idle purge mid-view.
	static constexpr int kLiveViewLingerMs = 250;

	mutable std::shared_mutex m_mutex;
	std::unordered_map<CompositeKey, TimedEntry, CompositeKeyHash> m_active;
	// steady_clock ns; live view is active while this is in the future. Written by
	// the render thread (SetLiveViewActive), read by the animation thread (Update).
	std::atomic<long long> m_liveViewUntilNs{ 0 };
};
