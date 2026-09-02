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
		// kAnimEvent only: the animation the event was fired from. Exact for
		// OAR-fired annotations (the replacement clip being walked). Engine-fired
		// events carry no clip identity, so those get the most recently activated
		// clip on that actor, prefixed with '~' to mark it a best guess.
		std::string sourceAnim;
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

	void AddAnimEvent(RE::TESObjectREFR* a_refr, const std::string& a_eventName,
		const std::string& a_sourceAnim = {});

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
	// Off until a log window is opened (UIAnimationLog / UIAnimationEventLog
	// OnOpen). Nothing functional reads the entries, and the event feed pays
	// string construction + a mutex per animation event on every actor while
	// this is on, so it is not worth paying for users who never open the logs.
	bool enabled{ false };
};
