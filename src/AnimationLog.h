#pragma once

class AnimationLog
{
public:
	enum class EventType
	{
		kActivate,
		kReplace,
		kLoop,
		kEcho,
		kAnimEvent,
	};

	struct Entry
	{
		EventType type;
		std::string refrName;
		uint32_t refrFormID{ 0 };
		std::string originalAnim;
		std::string replacementAnim;
		std::string subModName;
		std::chrono::steady_clock::time_point timestamp;
	};

	static AnimationLog* GetSingleton()
	{
		static AnimationLog singleton;
		return &singleton;
	}

	void AddEntry(EventType a_type, RE::TESObjectREFR* a_refr,
		const std::string& a_origAnim, const std::string& a_replAnim,
		const std::string& a_subModName);

	void AddAnimEvent(RE::TESObjectREFR* a_refr, const std::string& a_eventName);

	const std::deque<Entry>& GetEntries() const { return entries; }
	const std::deque<Entry>& GetAnimEventEntries() const { return animEventEntries; }
	void Clear();
	void ClearAnimEvents();
	void SetMaxEntries(int a_max) { maxEntries = a_max; }
	void SetEnabled(bool a_val) { enabled = a_val; }
	bool IsEnabled() const { return enabled; }

private:
	AnimationLog() = default;

	std::string GetRefrName(RE::TESObjectREFR* a_refr) const;

	mutable std::mutex mutex;
	std::deque<Entry> entries;

	mutable std::mutex animEventMutex;
	std::deque<Entry> animEventEntries;

	int maxEntries{ 100 };
	bool enabled{ true };
};
