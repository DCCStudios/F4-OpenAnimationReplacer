#pragma once

#include "Utils.h"
#include "HavokTypes.h"
#include "BaseConditions.h"
#include "Functions.h"
#include "Variants.h"
#include <atomic>
#include <mutex>

class ReplacementAnimation;

// Shape of the temporal blend ramp (blend in AND blend out). Every curve is an
// ease-in-out form mapping t in [0,1] to [0,1], monotonic and with f(0)=0,
// f(1)=1 — the blend code inverts them numerically when resuming a partially
// finished blend, which only works for monotonic curves.
// kQuadratic is the default because it is what OAR blended with before the
// curve became configurable.
enum class BlendCurve : int
{
	kLinear = 0,
	kQuadratic,
	kCubic,
	kHermiteCubic,
	kSinusoidal,
	kExponential,
};

struct ConditionPreset
{
	std::string name;
	std::string description;
	std::unique_ptr<ConditionSet> conditions;
};

class SubMod
{
public:
	SubMod(const std::string& a_name, int32_t a_priority, const std::filesystem::path& a_path)
		: name(a_name), priority(a_priority), path(a_path) {}

	const std::string& GetName() const { return name; }
	const std::string& GetDescription() const { return description; }
	int32_t GetPriority() const { return priority; }
	bool IsDisabled() const { return disabled; }
	bool IsInterruptible() const { return interruptible; }
	bool GetReplaceOnLoop() const { return replaceOnLoop; }
	bool GetReplaceOnEcho() const { return replaceOnEcho; }
	bool GetKeepRandomResultsOnLoop() const { return keepRandomResultsOnLoop; }
	bool GetShareRandomResults() const { return shareRandomResults; }
	bool GetReplaceAnnotations() const { return replaceAnnotations; }
	// True when the given annotation text must NOT be fired for this SubMod's
	// replacements. Matches the FULL annotation text case-insensitively (e.g.
	// "WeaponFire" or "SoundPlay.WPNRifleFire"). suppressAllAnnotations mutes
	// everything the replacement file carries.
	bool IsAnnotationSuppressed(const std::string& a_text) const
	{
		if (suppressAllAnnotations) return true;
		for (const auto& s : suppressedAnnotations) {
			if (s.size() == a_text.size() && _stricmp(s.c_str(), a_text.c_str()) == 0) {
				return true;
			}
		}
		return false;
	}
	float GetCustomBlendTimeOnInterrupt() const { return customBlendTimeOnInterrupt; }
	float GetCustomBlendTimeOnLoop() const { return customBlendTimeOnLoop; }
	float GetCustomBlendTimeOnEcho() const { return customBlendTimeOnEcho; }
	float GetCustomBlendOutTime() const { return customBlendOutTime; }
	float GetDeactivationDelay() const { return deactivationDelay; }
	void SetDeactivationDelay(float a_val) { deactivationDelay = a_val; }
	bool GetPlayOnceFullBody() const { return playOnceFullBody; }
	void SetPlayOnceFullBody(bool a_val) { playOnceFullBody = a_val; }
	bool GetEndClipIfShorter() const { return endClipIfShorter; }
	void SetEndClipIfShorter(bool a_val) { endClipIfShorter = a_val; }
	bool GetDisableIdleStop() const { return disableIdleStop; }
	void SetDisableIdleStop(bool a_val) { disableIdleStop = a_val; }
	bool GetLeafMatching() const { return leafMatching; }
	void SetLeafMatching(bool a_val) { leafMatching = a_val; }
	const std::string& GetRequiredProjectName() const { return requiredProjectName; }
	const std::string& GetOverrideAnimFolder() const { return overrideAnimationsFolder; }
	const std::filesystem::path& GetPath() const { return path; }
	ConditionSet* GetConditionSet() const { return conditionSet.get(); }
	const std::vector<ReplacementAnimation*>& GetReplacementAnimations() const { return replacementAnimations; }
	bool IsDirty() const { return dirty; }

	void SetName(const std::string& a_name) { name = a_name; }
	void SetDescription(const std::string& a_desc) { description = a_desc; }
	void SetPriority(int32_t a_priority) { priority = a_priority; }
	void SetDisabled(bool a_disabled) { disabled = a_disabled; }
	void SetInterruptible(bool a_val) { interruptible = a_val; }
	void SetReplaceOnLoop(bool a_val) { replaceOnLoop = a_val; }
	void SetReplaceOnEcho(bool a_val) { replaceOnEcho = a_val; }
	void SetKeepRandomResultsOnLoop(bool a_val) { keepRandomResultsOnLoop = a_val; }
	void SetShareRandomResults(bool a_val) { shareRandomResults = a_val; }
	void SetReplaceAnnotations(bool a_val) { replaceAnnotations = a_val; }
	void SetDirty(bool a_dirty) { dirty = a_dirty; }
	void SetConditionSet(std::unique_ptr<ConditionSet> a_set) { conditionSet = std::move(a_set); }
	void AddReplacementAnimation(ReplacementAnimation* a_anim) { replacementAnimations.push_back(a_anim); }

	bool EvaluateConditions(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) const;

	std::string name;
	std::string description;
	int32_t priority{ 0 };
	bool disabled{ false };
	bool interruptible{ false };
	bool replaceOnLoop{ true };
	bool replaceOnEcho{ true };
	bool keepRandomResultsOnLoop{ false };
	bool shareRandomResults{ false };
	bool replaceAnnotations{ true };
	// Annotation suppression: config "suppressAnnotations" accepts true (mute
	// ALL annotations of the replacement file) or an array of annotation names
	// to mute selectively. Only applies while this SubMod's replacement is
	// active with replaceAnnotations enabled (the default) — that's the mode
	// where OAR controls annotation emission.
	bool suppressAllAnnotations{ false };
	std::vector<std::string> suppressedAnnotations;
	float customBlendTimeOnInterrupt{ -1.0f };
	float customBlendTimeOnLoop{ -1.0f };
	float customBlendTimeOnEcho{ -1.0f };
	// Full-body blend-out duration when the replacement ends (conditions fail).
	// Negative = mirror the blend-in duration (existing behavior); 0 = instant.
	float customBlendOutTime{ -1.0f };
	// Ramp shape for this submod's blends — both the full-body blend and its
	// track filter's blend in/out.
	BlendCurve blendCurve{ BlendCurve::kQuadratic };
	float deactivationDelay{ 0.0f };
	bool playOnceFullBody{ false };
	// Full-body AND track-filtered replacements. When the replacement animation
	// is SHORTER than the original it replaces, the clip's state would otherwise
	// stay active for the original's (longer) duration — the original's leftover
	// tail plays out past the replacement's end (full body), or the source clip
	// keeps running past the donor's end (track filter). With this set, the
	// replacement's duration becomes authoritative for the clip's state, so the
	// clip ends when the replacement ends — as if the original were that length.
	// No original tail, no held frame.
	bool endClipIfShorter{ false };
	// Apply the IdleStopFix fast-forward before delivering the actor's next
	// IdleStop after an opted-in replacement. Disabled by default.
	bool disableIdleStop{ false };
	// Leaf matching: this submod's animations match by FILENAME alone. A
	// wpnmelee.hkx here replaces ANY clip whose animation file is named
	// wpnmelee.hkx, whatever folder it lives in — and it outranks exact-path
	// registrations (including direct-path resolved clips) whenever its
	// conditions pass. When they fail, normal path matching proceeds
	// untouched. Intended to be gated with conditions; without any, it
	// replaces every clip sharing the filename.
	bool leafMatching{ false };
	std::string requiredProjectName;
	std::string overrideAnimationsFolder;
	std::filesystem::path path;
	bool dirty{ false };
	bool hasUserConfig{ false };
	// True when a config.json exists on disk for this SubMod folder. A folder
	// with animations but no config.json still loads (always-matches, folder
	// name), but shows as "(no config)" in the UI with a "Create config.json"
	// action to formalize it. Set at parse time; set true after the UI writes
	// one so the tree stops flagging it without a full reload.
	bool hasConfig{ false };

	std::unique_ptr<ConditionSet> conditionSet;
	std::vector<ReplacementAnimation*> replacementAnimations;

	std::vector<std::unique_ptr<IFunction>> functionsOnActivate;
	std::vector<std::unique_ptr<IFunction>> functionsOnDeactivate;
	std::vector<std::unique_ptr<IFunction>> functionsOnTrigger;

	// Custom behavior events fired at replacement start/end.
	// These allow users to force-fire events the behavior graph needs
	// (e.g. ReloadEnd, SprintStop) that would otherwise be suppressed
	// when triggers are NULLed during annotation replacement.
	std::vector<std::string> eventsOnStart;
	std::vector<std::string> eventsOnEnd;

	// Partial body animation layering: only apply replacement to specific bones
	struct TrackFilter {
		bool enabled = false;
		enum class Mode { Override, Additive } mode = Mode::Additive;
		float weight = 1.0f;
		float blendInTime = 0.0f;
		float blendOutTime = 0.0f;
		// Where blendOutTime sits relative to the donor's final frame when a
		// one-shot overlay ends on its own.
		//   false (default): the fade COMPLETES on the final frame, so it
		//     starts blendOutTime before the end and the donor keeps animating
		//     underneath it — "blend out to the end of the animation".
		//   true (legacy): the fade STARTS on the final frame and runs past
		//     it, holding the donor's last pose while it ramps down.
		bool blendOutAtEnd = false;
		// Fixed-frame sampling: when >= 0, the replacement's pose for the
		// filtered bones is taken from this authored frame (30 fps) instead
		// of following the clip's playback time — a held static pose.
		// Negative = disabled (sample at the clip's current time).
		float sampleFrame = -1.0f;
		// Leaf-name prefixes whose source clips should be treated as continuous
		// loops even when the behavior graph exposes them as SINGLE_PLAY or
		// USER_CONTROLLED. Action clips remain one-shot unless explicitly listed.
		std::vector<std::string> loopSourcePrefixes;
		// Model-space anchoring (Override + playback-following only): the
		// filtered chain's ROOT bones are re-expressed so the chain lands
		// exactly where the donor animation puts it relative to the character
		// root, instead of playing donor locals under the base animation's
		// (different) torso pose. This is what makes "the left arm looks
		// exactly like the donor" true when only part of the body is filtered.
		bool modelSpaceAnchor = false;
		// Special-idle isolation: when SetupSpecialIdle/PlayIdle requests a
		// matching animation, do not start the engine's full-body idle. Advance
		// this donor on OAR's clock and stamp only the filtered tracks over the
		// normal graph. Default-off because callers may rely on native idle state.
		bool triggerOnlySpecialIdle = false;
		// Non-source clips normally receive the cached filtered pose so an
		// overlay remains stable across the other clips blended by the graph.
		// Additive support clips (typically names ending in "add") are unsafe
		// recipients for broad pose stamping: they carry native sway, pitch,
		// turn, and stance deltas. These perspective-specific defaults leave
		// those clips untouched while preserving direct replacement when an
		// *add clip is itself the source animation.
		bool skipAdditiveNonSourceFirstPerson = true;
		bool skipAdditiveNonSourceThirdPerson = true;
		bool includeChildren = true;
		std::vector<std::string> boneNames;
		// Exclude list: bones matching here are removed from the final resolved set
		bool excludeChildren = true;
		std::vector<std::string> excludeBoneNames;
		// Freeze list: these bones (and their children) are driven by NEITHER
		// the donor NOR the underlying animation while the overlay is active.
		// Each holds the local transform it had when the overlay started
		// (captured from the source clip, so there is no pop), released
		// through the normal blend-out. Exclusion alone means "the underlying
		// animation keeps driving the bone" - for a weapon gripped by
		// donor-driven hands that plays each weapon's native swing under the
		// overlay; freezing pins the grip instead.
		std::vector<std::string> freezeBoneNames;
		std::atomic<uint64_t> version{ 1 };
		std::mutex boneMutex;
	};
	TrackFilter trackFilter;

	// Variant animation configuration (loaded from JSON, applied to Variants objects)
	bool variantsEnabled{ true };
	VariantMode variantMode{ VariantMode::kRandom };
	VariantRerollPolicy variantRerollPolicy{ VariantRerollPolicy::kOnEachPlay };
	std::unordered_map<std::string, float> variantWeights;  // filename -> weight
};

class ReplacerMod
{
public:
	ReplacerMod(const std::string& a_name, const std::filesystem::path& a_path)
		: name(a_name), path(a_path) {}

	const std::string& GetName() const { return name; }
	const std::string& GetAuthor() const { return author; }
	const std::string& GetDescription() const { return description; }
	const std::filesystem::path& GetPath() const { return path; }

	void SetName(const std::string& a_name) { name = a_name; }
	void SetAuthor(const std::string& a_author) { author = a_author; }
	void SetDescription(const std::string& a_desc) { description = a_desc; }

	void AddSubMod(std::unique_ptr<SubMod> a_subMod)
	{
		subMods.push_back(std::move(a_subMod));
	}

	void SortSubMods()
	{
		std::ranges::sort(subMods, [](const auto& a, const auto& b) {
			return a->GetPriority() > b->GetPriority();
		});
	}

	const std::vector<std::unique_ptr<SubMod>>& GetSubMods() const { return subMods; }

	std::string name;
	std::string author;
	std::string description;
	std::filesystem::path path;
	// True when a config.json exists on disk for this Mod folder (holds only
	// name/author/description). A folder without one still loads under its
	// folder name; the UI offers "Create config.json" to name it.
	bool hasConfig{ false };
	std::vector<ConditionPreset> conditionPresets;
	std::vector<std::unique_ptr<SubMod>> subMods;
};
