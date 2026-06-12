#pragma once

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
	std::string actorName;
	uint32_t actorFormID{ 0 };
	bool conditionsPassed{ false };
	const SubMod* subMod{ nullptr }; // For live re-evaluation of conditions in the UI
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
		std::unique_lock lock(m_mutex);
		auto& timed = m_active[key];
		timed.entry = a_entry;
		timed.lastTouched = std::chrono::steady_clock::now();
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
		auto now = std::chrono::steady_clock::now();
		std::unique_lock lock(m_mutex);
		for (auto it = m_active.begin(); it != m_active.end();) {
			auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastTouched).count();
			if (age > 30000) {
				it = m_active.erase(it);
			} else {
				++it;
			}
		}
	}

private:
	mutable std::shared_mutex m_mutex;
	std::unordered_map<CompositeKey, TimedEntry, CompositeKeyHash> m_active;
};
