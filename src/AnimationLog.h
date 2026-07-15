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

	// Which animation graph the clip came from (used for 1st/3rd person filters).
	// Classified from the owning hkbCharacter's project data at log time.
	enum class Perspective : uint8_t
	{
		kUnknown,
		kFirstPerson,
		kThirdPerson,
	};

	struct Entry
	{
		EventType type;
		std::string refrName;
		uint32_t refrFormID{ 0 };
		std::string originalAnim;
		std::string replacementAnim;
		std::string subModName;
		// Full resolved on-disk path of the original animation (from the
		// subgraph swap-array resolution), when known. Display-only.
		std::string fullPath;
		Perspective perspective{ Perspective::kUnknown };
		std::chrono::steady_clock::time_point timestamp;
	};

	static AnimationLog* GetSingleton()
	{
		static AnimationLog singleton;
		return &singleton;
	}

	void AddEntry(EventType a_type, RE::TESObjectREFR* a_refr,
		const std::string& a_origAnim, const std::string& a_replAnim,
		const std::string& a_subModName,
		const std::string& a_fullPath = {},
		Perspective a_perspective = Perspective::kUnknown);

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

	int maxEntries{ 500 };
	bool enabled{ true };
};
