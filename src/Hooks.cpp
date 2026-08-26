#include "Hooks.h"
#include "Offsets.h"
#include "HavokTypes.h"
#include "Settings.h"
#include "ActiveClip.h"
#include "OpenAnimationReplacer.h"
#include "Parsing.h"
#include "ReplacerMods.h"
#include "AnimationCache.h"
#include "AnimationLog.h"
#include "ActiveReplacementTracker.h"
#include "RE_Additions.h"
#include "UI/BoneDebugViz.h"

#include <MinHook.h>

// Full declaration lives in Conditions.h; forward-declared here to avoid
// pulling the entire conditions header into this TU.
namespace ConditionTracking
{
	void TickPlayerAimState();
}
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <chrono>

static std::atomic<bool> s_gameFullyLoaded{ false };
static std::atomic<bool> s_hasActiveReplacements{ false };

// ActionFireEmpty detection — tracks when the engine dispatches the "fire empty" action
// to an actor's animation graph. Used by IsDryFiringCondition for reliable detection.
struct FireEmptyEntry {
	std::chrono::steady_clock::time_point timestamp;
	uint32_t generation = 0;  // increments each time fire-empty occurs
};
static std::shared_mutex s_fireEmptyMutex;
static std::unordered_map<uint32_t, FireEmptyEntry> s_fireEmptyMap;

// Nesting depth of IAnimationGraphManagerHolder::NotifyAnimationGraphImpl on this
// thread. Maintained by the Actor/PlayerCharacter vfunc hook. Replacement annotation
// firing consults this so it never nests a second notify inside an outer one
// (that path crashed collectActiveNodes with rdx=0 — crash-2026-07-23-00-04-17).
static thread_local int s_notifyAnimGraphDepth = 0;

bool WasFireEmptyRecent(uint32_t a_formID, int64_t a_windowMs)
{
	std::shared_lock lock{ s_fireEmptyMutex };
	auto it = s_fireEmptyMap.find(a_formID);
	if (it == s_fireEmptyMap.end()) return false;
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - it->second.timestamp).count();
	return elapsed < a_windowMs;
}

uint32_t GetFireEmptyGeneration(uint32_t a_formID)
{
	std::shared_lock lock{ s_fireEmptyMutex };
	auto it = s_fireEmptyMap.find(a_formID);
	if (it == s_fireEmptyMap.end()) return 0;
	return it->second.generation;
}

// Per-clip bypass set: clips that failed pre-swap in Activate are skipped in Update
// to prevent partial-state corruption when the animation control was built from a
// different animation than what OAR would try to swap in.
static std::shared_mutex s_bypassMutex;
static std::unordered_set<RE::hkbClipGenerator*> s_bypassSet;

// Per-play IdleStop suppression for special-idle replacements. The reference
// IdleStopFix plugin arms a global flag when SetupSpecialIdle runs; OAR already
// knows the exact winning replacement, so it can scope the same correlation to
// an actor, a clip, and an opted-in SubMod without fixed executable offsets.
struct IdleStopSuppressionArm
{
	uint32_t actorFormID{ 0 };
	SubMod* owner{ nullptr };
	std::string subModName;
	std::string originalPath;
};

static std::mutex s_idleStopSuppressionMutex;
static std::unordered_map<RE::hkbClipGenerator*, IdleStopSuppressionArm> s_idleStopArmedClips;
static std::unordered_map<uint32_t, std::pair<RE::hkbClipGenerator*, IdleStopSuppressionArm>> s_pendingIdleStopByActor;

static void UpdateIdleStopSuppressionArm(RE::hkbClipGenerator* a_clip, RE::TESObjectREFR* a_refr,
	const ReplacementAnimFileInfo* a_info)
{
	if (!a_clip || !a_refr || !a_info || !a_info->parentSubMod) return;

	auto* subMod = a_info->parentSubMod;
	const bool enabled = subMod->GetDisableIdleStop();

	std::lock_guard lock(s_idleStopSuppressionMutex);
	if (!enabled) {
		// Turning the option off while this exact play is active must cancel the
		// pending suppression. Condition-fail/deactivation paths deliberately do
		// not cancel it because IdleStop normally arrives after the clip ends.
		auto armIt = s_idleStopArmedClips.find(a_clip);
		if (armIt != s_idleStopArmedClips.end() && armIt->second.owner == subMod) {
			auto pendingIt = s_pendingIdleStopByActor.find(armIt->second.actorFormID);
			if (pendingIt != s_pendingIdleStopByActor.end() && pendingIt->second.first == a_clip) {
				s_pendingIdleStopByActor.erase(pendingIt);
			}
			s_idleStopArmedClips.erase(armIt);
		}
		return;
	}

	const auto actorFormID = a_refr->GetFormID();
	auto existing = s_idleStopArmedClips.find(a_clip);
	if (existing != s_idleStopArmedClips.end() &&
		existing->second.actorFormID == actorFormID &&
		existing->second.owner == subMod &&
		existing->second.originalPath == a_info->originalPath) {
		return;  // Same play; do not re-arm every Update after consumption.
	}

	IdleStopSuppressionArm arm;
	arm.actorFormID = actorFormID;
	arm.owner = subMod;
	arm.subModName = subMod->GetName();
	arm.originalPath = a_info->originalPath;
	s_idleStopArmedClips[a_clip] = arm;
	s_pendingIdleStopByActor[actorFormID] = { a_clip, arm };
	logger::info("[OAR-IdleStop] Armed actor {:X} from submod '{}' path='{}'",
		actorFormID, arm.subModName, arm.originalPath);
}

static void RearmIdleStopSuppressionForEcho(RE::hkbClipGenerator* a_clip, SubMod* a_activeSubMod)
{
	if (!a_clip || !a_activeSubMod) return;
	std::lock_guard lock(s_idleStopSuppressionMutex);
	auto it = s_idleStopArmedClips.find(a_clip);
	if (it == s_idleStopArmedClips.end() || it->second.owner != a_activeSubMod) return;
	s_pendingIdleStopByActor[it->second.actorFormID] = { a_clip, it->second };
	logger::info("[OAR-IdleStop] Re-armed echo for actor {:X} submod '{}'",
		it->second.actorFormID, it->second.subModName);
}

static bool ConsumeIdleStopSuppression(RE::TESObjectREFR* a_refr)
{
	if (!a_refr) return false;

	IdleStopSuppressionArm consumed;
	{
		std::lock_guard lock(s_idleStopSuppressionMutex);
		auto it = s_pendingIdleStopByActor.find(a_refr->GetFormID());
		if (it == s_pendingIdleStopByActor.end()) return false;
		consumed = it->second.second;
		s_pendingIdleStopByActor.erase(it);
	}

	logger::info("[OAR-IdleStop] Consumed fix arm for actor {:X} submod '{}' path='{}'",
		consumed.actorFormID, consumed.subModName, consumed.originalPath);
	return true;
}

static void ReleaseIdleStopSuppressionClip(RE::hkbClipGenerator* a_clip)
{
	if (!a_clip) return;
	std::lock_guard lock(s_idleStopSuppressionMutex);
	// Keep the actor-level pending entry: IdleStop is commonly delivered only
	// after the idle clip has deactivated. Its copied metadata is pointer-safe.
	s_idleStopArmedClips.erase(a_clip);
}

static void ClearIdleStopSuppressionState()
{
	std::lock_guard lock(s_idleStopSuppressionMutex);
	s_idleStopArmedClips.clear();
	s_pendingIdleStopByActor.clear();
}

void SetGameFullyLoaded(bool a_loaded) { s_gameFullyLoaded.store(a_loaded); }
void SetHasActiveReplacements(bool a_has) { s_hasActiveReplacements.store(a_has); }
bool HasActiveReplacements() { return s_hasActiveReplacements.load(); }

static std::shared_mutex s_characterCacheMutex;
static std::unordered_map<RE::hkbCharacter*, RE::TESObjectREFR*> s_characterCache;
static std::unordered_set<RE::hkbCharacter*> s_mainBodyCharacters;

static RE::hkbCharacterStringData* s_capturedStringData{ nullptr };
static std::string s_capturedAnimPath;
static std::mutex s_capturedMutex;
static std::vector<void*> s_capturedGraphs;
static std::mutex s_capturedGraphsMutex;

// LoadClips path map: maps stringData -> animation folder path (populated by Hook #1)
static std::shared_mutex s_loadClipsPathMutex;
static std::unordered_map<RE::hkbCharacterStringData*, std::string> s_loadClipsPathMap;

// All known stringData objects from player's graphs (populated at post-load)
static std::shared_mutex s_knownStringDataMutex;
static std::vector<RE::hkbCharacterStringData*> s_knownStringDataList;

// ============ Option 1: Weapon graph projectData animationPath ============
// Cached per-graph animationPath from PlayerCharacter's BSAnimationGraphManager.
// Key: graph index (0=body, 1=weapon typically). Value: normalized animation folder prefix.
static std::shared_mutex s_graphAnimPathMutex;
static std::unordered_map<uint32_t, std::string> s_graphAnimPathByIndex;  // graph index -> folder prefix
static std::string s_weaponAnimFolder;  // latest weapon graph's folder (e.g. "scar")
static std::atomic<bool> s_weaponAnimFolderValid{ false };

// ============ Option 2: CreateFileW animation path capture ============
// Maps leaf animation name -> set of folder prefixes seen in actual file opens.
// E.g. "wpnreload" -> {"scar", "44pistol", "1911"}
static std::shared_mutex s_createFileAnimMutex;
static std::unordered_map<std::string, std::unordered_set<std::string>> s_createFileLeafToFolders;
// Most recent folder seen for each leaf (temporal proximity hint)
static std::unordered_map<std::string, std::string> s_createFileLeafToLatestFolder;
// Full set of captured paths: "scar\wpnreload" etc.
static std::unordered_set<std::string> s_createFileCapturedPaths;

// ============ Option 3: currentWeaponSubGraphID ============
static std::atomic<uint64_t> s_lastWeaponSubGraphID{ 0 };
static std::shared_mutex s_subGraphToFolderMutex;
static std::unordered_map<uint64_t, std::string> s_subGraphIDToFolder;  // subgraph identifier -> folder prefix

// Source 2: LoadedIdleAnimData reverse map (clipGenerator* -> animFile)
static std::shared_mutex s_idleAnimReverseMutex;
static std::unordered_map<RE::hkbClipGenerator*, std::string> s_idleAnimReverseMap;
static std::atomic<bool> s_idleAnimReverseBuilt{ false };

// ============ Partial Body Animation Layering ============
// Per-actor state for filtered (partial body) replacements. Keyed by TESObjectREFR*
// so the filter persists across ALL hkbCharacters that belong to the same actor
// (e.g. 1stperson body, 3rdperson body, weapon graph). Each character resolves bone
// indices against ITS OWN skeleton and the cached replacement pose is keyed by bone
// NAME so it's portable across different skeletons.

// Per-character bone resolution: ordered list of (bone-name, bone-index) pairs for
// this character's skeleton, expanded by includeChildren if requested.
struct CharResolved {
	std::vector<std::pair<std::string, int16_t>> nameAndIndex;
	// Frozen bones (freezeBoneNames + children), resolved on this skeleton.
	// Disjoint from nameAndIndex: freeze wins over the donor include set.
	std::vector<std::pair<std::string, int16_t>> freezeNameAndIndex;
	uint64_t version = 0; // matches TrackFilter::version when this was resolved
};

struct CharTrackFilterState {
	RE::hkaAnimation* replacement = nullptr;
	RE::hkaAnimation* sourceAnimation = nullptr; // Original animation the source clip plays (for blend-sibling identification)
	SubMod::TrackFilter* filter = nullptr;
	SubMod* parentSubMod = nullptr;
	RE::hkbClipGenerator* sourceClip = nullptr;
	std::unordered_set<RE::hkbClipGenerator*> sourceClips;
	std::string suffix;
	// Filter-only special idles have no native source clip. Their donor is
	// advanced from this wall-clock origin and sampled into one ordinary,
	// non-additive graph output per actor update.
	bool standaloneSpecialIdle = false;
	float standaloneStartSec = 0.0f;
	uint64_t lastStandaloneSampleFrame = UINT64_MAX;

	// Cache rep/base pose by bone NAME so it's portable across 1p/3p skeletons.
	std::unordered_map<std::string, RE::hkQsTransformRaw> cachedRepByName;
	std::unordered_map<std::string, RE::hkQsTransformRaw> cachedBaseByName;
	bool cacheValid = false;

	// Track-filtered Camera motion is transferred as a model-space delta from the
	// donor's frame-zero pose, never as a portable absolute local transform. Keep
	// the complete frame-zero sample because reconstructing the Camera model
	// transform requires every animated ancestor in its skeleton chain.
	std::vector<RE::hkQsTransformRaw> cameraDonorFrameZeroTracks;
	std::unordered_set<int32_t> invalidCameraReferenceTracks;
	// The Generate hook samples Camera with the rest of the donor, but it never
	// writes Camera back into Havok's output pose. Instead it publishes the
	// weighted local delta for the current PlayerCharacter::UpdateAnimation pass.
	// The post-animation hook consumes that delta on the actual first-person
	// Camera scene node, after the graph has finished evaluating.
	float pendingCameraTranslationDelta[3]{ 0.0f, 0.0f, 0.0f };
	float pendingCameraRotationDelta[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
	uint64_t pendingCameraEvaluation = 0;
	bool pendingCameraValid = false;

	// Frozen-bone locals, captured from the SOURCE clip's own pose on the
	// first sample of each play (so the freeze starts exactly where the
	// underlying animation left the bone - no pop) and stamped into every
	// clip at the overlay's weight until the play ends. Cleared on every
	// fresh play/restart so a weapon switch re-captures the new grip.
	std::unordered_map<std::string, RE::hkQsTransformRaw> frozenByName;

	// Per-character bone-name → index resolution (rebuilt when filter version changes).
	std::unordered_map<RE::hkbCharacter*, CharResolved> resolvedByChar;

	// The donor file's OWN track->bone map (from its hkaAnimationBinding),
	// captured at registration. Sampling MUST use this when present: the host
	// clip's binding describes the HOST animation's track layout, which only
	// matches the donor's on the donor's own weapon. donorMapIdentity means
	// the binding declared identity mapping (empty index array). Empty map
	// with identity=false -> no binding found, host mapping fallback.
	std::vector<int16_t> donorTrackToBone;
	bool donorMapIdentity = false;
	bool donorMapQueried = false;

	// Wall-clock time (seconds, from s_tfTimeSec) of the last source-clip
	// Generate. Used to detect that source clips stopped generating (without
	// firing a Deactivate hook). Time-based, NOT frame-based, so the staleness
	// window is identical at any framerate.
	float lastSourceTimeSec = 0.0f;

	// --- One-shot playback tracking ---
	// Only meaningful for playback-following filters (sampleFrame < 0). Fixed-frame
	// pose donors keep the original persistent-overlay semantics untouched.
	//
	// Wall-clock time of the last actual donor SAMPLE. Unlike lastSourceTimeSec
	// this is never refreshed by mere re-registration (the Update hook re-registers
	// every frame while the clip generator stays alive, which is exactly what kept
	// a finished grenade-throw overlay pinned forever, 2026-08-04 session).
	float lastSampleSec = 0.0f;
	// Source-clip localTime at the last sample. A backward jump means the engine
	// restarted the clip for a new play — the overlay re-arms and blends back in.
	float lastSampledLocalTime = -1.0f;
	// Wall-clock time localTime last ADVANCED. The graph can park a finished
	// clip at a fixed localTime while keeping it active and sampling — the
	// stall detector ends the one-shot instead of freezing on that pose.
	float lastAdvanceSec = 0.0f;
	// Donor self-advance (one-shot sources only): when the SOURCE clip finishes
	// or parks before the donor animation has reached its own end, the donor
	// keeps playing on wall-clock time from the moment the source parked,
	// instead of ending the overlay early (KV Broadside grenade throw: 1.29s
	// vanilla clip cut a longer donor's tail off) or freezing on the parked
	// frame (the pre-2026-08-04 behavior). -1 = following source playback.
	float selfAdvanceStartSec = -1.0f;
	// Donor localTime at the moment self-advance began.
	float selfAdvanceBaseTime = 0.0f;
	// End-anchored blend-out (TrackFilter::blendOutAtEnd == false, the default):
	// the donor has reached blendOutTime before its end, so the fade starts NOW
	// in order to finish exactly on the final frame. Distinct from oneShotDone,
	// which clamps localTime and holds the last pose — here the donor keeps
	// animating normally underneath the fade.
	bool earlyBlendOutArmed = false;
	// Donor playback reached its end for the current activation (non-looping
	// source): the tick updater blends the overlay out even though conditions
	// may still be true, and re-registration must not cancel that blend-out.
	bool oneShotDone = false;
	// One-shot finished and fully blended out. The state is kept (alpha 0, applies
	// nothing) so a restart of the same clip generator can blend back in without
	// the erase-then-recreate cycle re-sampling the held end pose.
	bool dormant = false;
	// The source stopped producing samples (clip zero-weight / ended before the
	// donor). Unlike oneShotDone this clears the moment samples resume — a
	// looping source (sprint overlay) that pauses must come back on its own.
	bool sampleStarved = false;
	// Guards duplicate onEnd custom-event delivery: fired once at dormant entry
	// OR at erase, never both.
	bool onEndFired = false;

	// Temporal blend state: ramps effectiveAlpha toward 1.0 (active) or 0.0 (deactivating)
	float blendAlpha = 0.0f;        // current interpolated alpha [0, 1]
	bool blendingOut = false;        // true = ramping down, erase when alpha reaches 0
	float blendElapsed = 0.0f;       // seconds elapsed since blend started
	float blendDuration = 0.0f;      // target duration for current blend direction

	// Deactivation delay: holds the filter active for N seconds after conditions become false
	bool deactivationDelayActive = false;
	float deactivationDelayRemaining = 0.0f;
};
static std::shared_mutex s_trackFilterMutex;
// Multiple track-filtered submods can be active on one actor at the same time
// (e.g. a bolt-lock filter from "Idle Empty" plus an arms filter from "Super
// Sprint"). Each gets its own independent state — sharing a single slot per
// actor made concurrent filters evict each other and skip blend-in (the slot
// was "not new", so blendAlpha stayed at the previous filter's value).
static std::unordered_map<RE::TESObjectREFR*, std::vector<CharTrackFilterState>> s_charTrackFilterMap;
static std::atomic<int> s_trackFilterActiveCount{ 0 };

// A monotonically increasing token identifies one PlayerCharacter animation
// evaluation. Generate hooks reached from that evaluation publish Camera
// deltas with this token, so the post-animation scene-node hook cannot replay a
// stale sample when a filter stops generating or a graph is rebuilt.
static std::atomic<uint64_t> s_cameraEvaluationSerial{ 0 };
static std::atomic<uint64_t> s_activeCameraEvaluation{ 0 };

struct AppliedTrackFilterCamera
{
	RE::NiAVObject* root = nullptr;
	RE::NiAVObject* node = nullptr;
	RE::NiTransform base = RE::NiTransform::IDENTITY;
	RE::NiTransform applied = RE::NiTransform::IDENTITY;
	bool valid = false;
};

// PlayerCharacter::UpdateAnimation runs on the game thread. This state is only
// touched immediately before and after that call, so it deliberately does not
// need a second lock alongside s_trackFilterMutex.
static AppliedTrackFilterCamera s_appliedTrackFilterCamera;

// Find the state for a specific filter on an actor. Returns nullptr if absent.
// Caller must hold s_trackFilterMutex (shared or unique).
static CharTrackFilterState* FindTrackFilterState(RE::TESObjectREFR* a_actor, const SubMod::TrackFilter* a_filter)
{
	auto it = s_charTrackFilterMap.find(a_actor);
	if (it == s_charTrackFilterMap.end()) return nullptr;
	for (auto& state : it->second) {
		if (state.filter == a_filter) return &state;
	}
	return nullptr;
}

// Play Once (Full Body): tracks the initial replacement decision per clip generator.
// When a clip has a playOnceFullBody candidate, the first evaluation result is locked
// so that mid-animation condition flips in either direction are ignored.
//
// The decision records WHICH SubMod won, not just whether to replace: locked
// replays used to re-derive the winner as "first playOnceFullBody candidate",
// which silently overrode a higher-priority non-play-once winner (SCAR ADS
// Reload prio 3002 lost every reload to SCAR Reload Variants prio 3000+
// playOnce, 2026-08-03 session).
struct PlayOnceDecision
{
	bool replace{ false };
	SubMod* winner{ nullptr };  // valid while replace is true; cleared with the map on config reload
};
static std::shared_mutex s_playOnceDecisionMutex;
static std::unordered_map<RE::hkbClipGenerator*, PlayOnceDecision> s_playOnceDecision;

// Per-frame counter, incremented in HookedActorUpdate. Used for staleness detection.
static std::atomic<uint64_t> s_currentFrame{ 0 };
// Wall-clock seconds since plugin init, updated once per HookedActorUpdate.
// Track filter lifetime decisions use THIS, never frame counts — a frame-count
// window shrinks in wall time as the framerate rises (300 frames is 5s at
// 60fps but ~1.5s at 200fps), which made cached overrides drop out early on
// fast/inconsistent framerates.
static std::atomic<float> s_tfNowSec{ 0.0f };
// Threshold: if no source clip has fired Generate for this long, the entry
// is considered stale and erased (so non-source clips stop applying old cached pose).
static constexpr float kTrackFilterStaleSeconds = 5.0f;
// Playback-following filters only: if the source hasn't produced an actual donor
// SAMPLE for this long (clip went zero-weight or ended before the donor did),
// the overlay blends out. Long enough to ride out brief graph transitions,
// short enough that a finished one-shot doesn't visibly linger.
static constexpr float kOneShotSampleGraceSeconds = 0.5f;
// One-shot sources only: localTime not advancing for this long while samples
// keep flowing means the graph parked the finished clip — end the play.
static constexpr float kOneShotStallSeconds = 0.25f;

// --- Full-body replacement blend state ---
// Uses a cached pose snapshot so we NEVER call _Generate twice per frame.
// Keyed by (actor, clip suffix).
struct FullBodyBlendState {
	RE::hkaAnimation* replacement = nullptr;
	RE::hkaAnimation* original = nullptr;
	RE::hkbClipGenerator* ownerClip = nullptr; // only this clip applies blending
	float blendAlpha = 0.0f;       // 0 = fully original, 1 = fully replacement
	float blendElapsed = 0.0f;
	float blendDuration = 0.0f;
	// SubMod's configured blend-out duration, captured at registration.
	// Negative = mirror the blend-in duration (default behavior).
	float blendOutDuration = -1.0f;
	// Ramp shape, captured alongside blendOutDuration: this state outlives the
	// evaluation that created it, and the owning SubMod pointer is not safe to
	// dereference here after a config reload.
	BlendCurve blendCurve = BlendCurve::kQuadratic;
	bool blendingIn = false;       // ramping 0→1
	bool blendingOut = false;      // ramping 1→0
	bool poseSnapshotValid = false;
	std::vector<RE::hkQsTransformRaw> poseSnapshot; // frozen pose from the "other" side
};
struct ActorClipKey {
	RE::TESObjectREFR* actor = nullptr;
	std::string suffix;
	bool operator==(const ActorClipKey& o) const { return actor == o.actor && suffix == o.suffix; }
};
struct ActorClipKeyHash {
	size_t operator()(const ActorClipKey& k) const {
		size_t h1 = std::hash<void*>{}(k.actor);
		size_t h2 = std::hash<std::string>{}(k.suffix);
		return h1 ^ (h2 << 1);
	}
};
static std::shared_mutex s_fullBodyBlendMutex;
static std::unordered_map<ActorClipKey, FullBodyBlendState, ActorClipKeyHash> s_fullBodyBlendMap;
static std::atomic<int> s_fullBodyBlendActiveCount{ 0 };

// ---- Easing for temporal blend ----

static float EaseInOutQuad(float t)
{
	if (t <= 0.0f) return 0.0f;
	if (t >= 1.0f) return 1.0f;
	return t < 0.5f ? 2.0f * t * t : t * (4.0f - 2.0f * t) - 1.0f;
}

// Configurable ramp shape (SubMod::blendCurve). All are ease-in-out forms:
// monotonic on [0,1] with f(0)=0 and f(1)=1, which is what makes the numeric
// inverse below valid.
static float EvaluateBlendCurve(BlendCurve a_curve, float t)
{
	if (t <= 0.0f) return 0.0f;
	if (t >= 1.0f) return 1.0f;

	switch (a_curve) {
	case BlendCurve::kLinear:
		return t;
	case BlendCurve::kCubic:
		return t < 0.5f
			? 4.0f * t * t * t
			: 1.0f - 0.5f * std::pow(-2.0f * t + 2.0f, 3.0f);
	case BlendCurve::kHermiteCubic:
		// Classic smoothstep: zero first derivative at both ends.
		return t * t * (3.0f - 2.0f * t);
	case BlendCurve::kSinusoidal:
		return 0.5f * (1.0f - std::cos(t * 3.14159265358979323846f));
	case BlendCurve::kExponential:
		return t < 0.5f
			? 0.5f * std::pow(2.0f, 20.0f * t - 10.0f)
			: 1.0f - 0.5f * std::pow(2.0f, -20.0f * t + 10.0f);
	case BlendCurve::kQuadratic:
	default:
		return EaseInOutQuad(t);
	}
}

// Solve EvaluateBlendCurve(curve, t) == a_alpha for t. Used when a blend starts
// from a partially-blended alpha (cancelling a blend-out mid-ramp, re-arming a
// fade) so the ramp resumes at the right place instead of popping. Bisection
// rather than closed forms: it is one loop for every curve, cannot go unstable,
// and only runs when a blend begins — never per frame.
static float InverseBlendCurve(BlendCurve a_curve, float a_alpha)
{
	if (a_alpha <= 0.0f) return 0.0f;
	if (a_alpha >= 1.0f) return 1.0f;
	if (a_curve == BlendCurve::kLinear) return a_alpha;

	float lo = 0.0f;
	float hi = 1.0f;
	for (int i = 0; i < 16; ++i) {
		const float mid = 0.5f * (lo + hi);
		if (EvaluateBlendCurve(a_curve, mid) < a_alpha) {
			lo = mid;
		} else {
			hi = mid;
		}
	}
	return 0.5f * (lo + hi);
}

static BlendCurve CurveOf(const SubMod* a_subMod)
{
	return a_subMod ? a_subMod->blendCurve : BlendCurve::kQuadratic;
}

// ---- Quaternion math helpers for bone blending ----

static float QuatDot(const float* a, const float* b)
{
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
}

static void NormalizeQuat(float* q)
{
	const float lenSq = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
	if (lenSq < 1e-12f) {
		// Degenerate input — fall back to identity rather than emitting NaNs
		// into a pose the engine is about to render.
		q[0] = q[1] = q[2] = 0.0f;
		q[3] = 1.0f;
		return;
	}
	const float inv = 1.0f / std::sqrt(lenSq);
	for (int i = 0; i < 4; ++i) q[i] *= inv;
}

// Shortest-path slerp. q and -q are the same orientation (quaternion double
// cover), so a negative dot means the raw interpolation would travel the LONG
// way around — up to 360 degrees of unwanted spin through a bone. Flipping the
// sign of b in that case is what guarantees the short arc.
//
// The result is normalized unconditionally: the near-parallel branch is a plain
// lerp (nlerp), which lands slightly inside the unit sphere, and every additive
// blend feeds its output back through MultiplyQuat, so without this the drift
// compounds frame over frame into a visibly shrinking/skewing bone.
static void SlerpQuat(const float* a, const float* b, float t, float* out)
{
	float dot = QuatDot(a, b);
	float sign = 1.0f;
	if (dot < 0.0f) { dot = -dot; sign = -1.0f; }
	if (dot > 1.0f) dot = 1.0f;

	float s0, s1;
	if (dot > 0.9995f) {
		s0 = 1.0f - t;
		s1 = t * sign;
	} else {
		float theta = acosf(dot);
		float sinTheta = sinf(theta);
		if (sinTheta < 1e-6f) { s0 = 1.0f - t; s1 = t * sign; }
		else { s0 = sinf((1.0f - t) * theta) / sinTheta; s1 = sinf(t * theta) / sinTheta * sign; }
	}
	for (int i = 0; i < 4; ++i)
		out[i] = a[i] * s0 + b[i] * s1;
	NormalizeQuat(out);
}

static void MultiplyQuat(const float* p, const float* q, float* out)
{
	float r[4];
	r[0] = p[3]*q[0] + p[0]*q[3] + p[1]*q[2] - p[2]*q[1];
	r[1] = p[3]*q[1] - p[0]*q[2] + p[1]*q[3] + p[2]*q[0];
	r[2] = p[3]*q[2] + p[0]*q[1] - p[1]*q[0] + p[2]*q[3];
	r[3] = p[3]*q[3] - p[0]*q[0] - p[1]*q[1] - p[2]*q[2];
	for (int i = 0; i < 4; ++i) out[i] = r[i];
}

static void InverseQuat(const float* q, float* out)
{
	out[0] = -q[0]; out[1] = -q[1]; out[2] = -q[2]; out[3] = q[3];
}

// ---- Binding helpers ----

// Read the clip's "active" hkaAnimationBinding (the one used by Generate when
// it samples). Two locations may hold a binding pointer at runtime:
//   1. hkbClipGenerator + 0xE8  ("file/structural" binding; can be null after
//                               post-activation cleanup)
//   2. animationControl + 0x38 ("live" binding used by SamplePartialTracks;
//                               this is what GetAnimationSlot follows)
// We prefer (2) because that's the binding the engine actually consults when
// sampling. Fall back to (1) only if (2) is missing.
static uint8_t* GetActiveBindingBytes(const RE::hkbClipGenerator* a_clip)
{
	if (!a_clip) return nullptr;

	// Try animationControl path first (matches GetAnimationSlot() in HavokTypes.h)
	auto* ctrl = reinterpret_cast<uint8_t*>(a_clip->GetAnimationControlRaw());
	if (ctrl) {
		auto* bindFromCtrl = *reinterpret_cast<uint8_t**>(ctrl + 0x38);
		if (bindFromCtrl) return bindFromCtrl;
	}

	// Fallback: clip's own binding slot
	return reinterpret_cast<uint8_t*>(a_clip->GetBindingRaw());
}

// Read the binding's transformTrackToBoneIndices array. Returns nullptr if no
// binding is currently associated with the clip.
//   binding+0x18: hkRefPtr<hkaAnimation> animation
//   binding+0x20: hkArray<int16_t>       transformTrackToBoneIndices
static const RE::hkArrayRawLayout* GetTrackToBoneIndices(const RE::hkbClipGenerator* a_clip)
{
	auto* bindingBytes = GetActiveBindingBytes(a_clip);
	if (!bindingBytes) return nullptr;
	return reinterpret_cast<const RE::hkArrayRawLayout*>(
		bindingBytes + RE::kBindingOffset_transformTrackToBoneIndices);
}

// Find the TRACK index in `a_clip`'s binding that maps to the given skeleton
// bone index. Returns -1 if no track in the binding maps to that bone (e.g.,
// the bone has no animation track on this clip).
static int32_t FindTrackIndexForBone(const RE::hkbClipGenerator* a_clip, int16_t a_boneIdx)
{
	const auto* arr = GetTrackToBoneIndices(a_clip);
	if (!arr || !arr->data || arr->size <= 0) return -1;
	const auto* indices = reinterpret_cast<const int16_t*>(arr->data);
	for (int32_t t = 0; t < arr->size; ++t) {
		if (indices[t] == a_boneIdx) return t;
	}
	return -1;
}

// Override: lerp base toward replacement absolute pose
static void LerpTransform(RE::hkQsTransformRaw& base, const RE::hkQsTransformRaw& rep, float w)
{
	float iw = 1.0f - w;
	for (int i = 0; i < 4; ++i)
		base.translation[i] = base.translation[i] * iw + rep.translation[i] * w;
	SlerpQuat(base.rotation, rep.rotation, w, base.rotation);
	for (int i = 0; i < 4; ++i)
		base.scale[i] = base.scale[i] * iw + rep.scale[i] * w;
}

// --- hkQsTransform algebra for model-space anchoring ---
// Havok quaternions are (x, y, z, w) with w at [3]. The inverse assumes
// uniform scale (FO4 character animations do not animate scale), and
// normalized quaternions (Havok pose data is normalized).

static RE::hkQsTransformRaw MakeIdentityQs()
{
	RE::hkQsTransformRaw t{};
	t.rotation[3] = 1.0f;
	t.scale[0] = t.scale[1] = t.scale[2] = 1.0f;
	return t;
}

static bool IsTrackFilterCameraBone(std::string_view a_name)
{
	return a_name.size() == 6 && _strnicmp(a_name.data(), "Camera", 6) == 0;
}

static bool IsFiniteQs(const RE::hkQsTransformRaw& a_transform)
{
	for (int i = 0; i < 4; ++i) {
		if (!std::isfinite(a_transform.translation[i]) ||
			!std::isfinite(a_transform.rotation[i]) ||
			!std::isfinite(a_transform.scale[i])) {
			return false;
		}
	}
	return true;
}

static bool HasUsableRotation(const RE::hkQsTransformRaw& a_transform)
{
	const float lengthSquared = QuatDot(a_transform.rotation, a_transform.rotation);
	return std::isfinite(lengthSquared) && lengthSquared > 1e-8f;
}

static RE::NiMatrix3 CameraQuatToMatrix(const float a_quaternion[4])
{
	float q[4] = {
		a_quaternion[0], a_quaternion[1], a_quaternion[2], a_quaternion[3]
	};
	NormalizeQuat(q);

	const float x = q[0];
	const float y = q[1];
	const float z = q[2];
	const float w = q[3];
	return RE::NiMatrix3(
		1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y - z * w), 2.0f * (x * z + y * w), 0.0f,
		2.0f * (x * y + z * w), 1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z - x * w), 0.0f,
		2.0f * (x * z - y * w), 2.0f * (y * z + x * w), 1.0f - 2.0f * (x * x + y * y), 0.0f);
}

static bool CameraTransformNear(const RE::NiTransform& a_left, const RE::NiTransform& a_right)
{
	constexpr float kTranslationEpsilon = 1e-4f;
	constexpr float kRotationEpsilon = 1e-5f;
	constexpr float kScaleEpsilon = 1e-5f;
	for (std::size_t row = 0; row < 3; ++row) {
		for (std::size_t column = 0; column < 3; ++column) {
			if (std::abs(a_left.rotate[row][column] - a_right.rotate[row][column]) > kRotationEpsilon) {
				return false;
			}
		}
	}
	return std::abs(a_left.translate.x - a_right.translate.x) <= kTranslationEpsilon &&
		std::abs(a_left.translate.y - a_right.translate.y) <= kTranslationEpsilon &&
		std::abs(a_left.translate.z - a_right.translate.z) <= kTranslationEpsilon &&
		std::abs(a_left.scale - a_right.scale) <= kScaleEpsilon;
}

static void QuatMul(const float a[4], const float b[4], float out[4])
{
	// out = a * b (b's rotation applied first, then a's)
	const float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
	const float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
	const float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
	const float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
	out[0] = x; out[1] = y; out[2] = z; out[3] = w;
}

static void QuatRotateVec(const float q[4], const float v[3], float out[3])
{
	// v' = v + w*t + q.xyz × t, where t = 2*(q.xyz × v)
	const float tx = 2.0f * (q[1] * v[2] - q[2] * v[1]);
	const float ty = 2.0f * (q[2] * v[0] - q[0] * v[2]);
	const float tz = 2.0f * (q[0] * v[1] - q[1] * v[0]);
	const float rx = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
	const float ry = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
	const float rz = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
	out[0] = rx; out[1] = ry; out[2] = rz;
}

// out = p ∘ c (parent transform applied to child): x -> p.t + p.R*(p.s * c(x)).
// Alias-safe: out may be p or c.
static void ComposeQs(const RE::hkQsTransformRaw& p, const RE::hkQsTransformRaw& c, RE::hkQsTransformRaw& out)
{
	RE::hkQsTransformRaw r{};
	const float scaled[3] = {
		c.translation[0] * p.scale[0],
		c.translation[1] * p.scale[1],
		c.translation[2] * p.scale[2]
	};
	float rotated[3];
	QuatRotateVec(p.rotation, scaled, rotated);
	r.translation[0] = p.translation[0] + rotated[0];
	r.translation[1] = p.translation[1] + rotated[1];
	r.translation[2] = p.translation[2] + rotated[2];
	r.translation[3] = 0.0f;
	QuatMul(p.rotation, c.rotation, r.rotation);
	r.scale[0] = p.scale[0] * c.scale[0];
	r.scale[1] = p.scale[1] * c.scale[1];
	r.scale[2] = p.scale[2] * c.scale[2];
	r.scale[3] = 0.0f;
	out = r;
}

// out = t⁻¹ under the same application form (uniform-scale assumption).
// Alias-safe: out may be t.
static void InverseQs(const RE::hkQsTransformRaw& t, RE::hkQsTransformRaw& out)
{
	RE::hkQsTransformRaw r{};
	r.rotation[0] = -t.rotation[0];
	r.rotation[1] = -t.rotation[1];
	r.rotation[2] = -t.rotation[2];
	r.rotation[3] = t.rotation[3];
	float rt[3];
	const float tr[3] = { t.translation[0], t.translation[1], t.translation[2] };
	QuatRotateVec(r.rotation, tr, rt);
	for (int i = 0; i < 3; ++i) {
		const float s = t.scale[i];
		const float si = (std::fabs(s) > 1e-6f) ? 1.0f / s : 1.0f;
		r.scale[i] = si;
		r.translation[i] = -rt[i] * si;
	}
	r.translation[3] = 0.0f;
	r.scale[3] = 0.0f;
	out = r;
}

// Set the "modified bones" bitmask bit for a bone in the pose track's output.
//
// The pose track in hkbGeneratorOutput stores pose data followed by a bitmask
// indicating which bones have valid (modified) data. The engine uses this mask
// during pose composition: bones whose bit is 0 are treated as "no data" and
// their values are ignored / replaced by the next layer's contribution.
//
// When we override outputPose[idx] for a bone whose original animation does NOT
// have a track for that bone (e.g., player idle has no WeaponBolt track), the
// engine's Generate leaves the mask bit 0. Our pose data write is then ignored
// at downstream processing. Setting the mask bit explicitly tells the engine
// "this bone is modified, please honor it".
//
// Layout (per OAR Skyrim's ActiveAnimationPreview / Havok 2014):
//   [tracksPtr + poseHeader.dataOffset]                              ← pose data start
//   ...                                                              hkQsTransform[capacity]
//   [tracksPtr + poseHeader.dataOffset + elementSizeBytes*capacity]  ← bone mask start
//   uint32_t mask[(capacity + 32) >> 5]
static void SetPoseBoneMaskBit(uint8_t* a_tracksPtr,
	const RE::TrackHeaderRaw& a_poseHeader, int16_t a_boneIdx)
{
	if (!a_tracksPtr || a_boneIdx < 0) return;
	if (a_poseHeader.elementSizeBytes <= 0 || a_poseHeader.capacity <= 0) return;
	if (a_boneIdx >= a_poseHeader.capacity) return;

	// The bone mask only exists for sparse tracks (flags & 0x02).
	// Dense tracks have all bones "active" — no mask to write.
	if ((a_poseHeader.flags & 0x02) == 0) return;

	auto maskOffset = static_cast<uintptr_t>(a_poseHeader.dataOffset)
		+ static_cast<uintptr_t>(a_poseHeader.elementSizeBytes) * a_poseHeader.capacity;
	auto maskWordOffset = maskOffset + static_cast<uintptr_t>(a_boneIdx >> 5) * 4;

	// Bounds check against total buffer size (TrackMasterHeaderRaw::numBytes)
	auto* master = reinterpret_cast<RE::TrackMasterHeaderRaw*>(a_tracksPtr);
	if (static_cast<int32_t>(maskWordOffset + 4) > master->numBytes) return;

	auto* maskBytes = a_tracksPtr + maskOffset;
	auto* mask = reinterpret_cast<uint32_t*>(maskBytes);
	mask[a_boneIdx >> 5] |= (1u << (a_boneIdx & 0x1F));
}

// Additive: given the base pose from the original animation (origBase) and the
// replacement pose (rep), compute delta = rep - origBase, then apply delta*weight
// on top of whatever current output is. This lets additive work with full-pose
// replacement animations, computing a proper offset.
static void BlendAdditiveTransform(RE::hkQsTransformRaw& output,
	const RE::hkQsTransformRaw& origBase, const RE::hkQsTransformRaw& rep, float w)
{
	for (int i = 0; i < 3; ++i)
		output.translation[i] += (rep.translation[i] - origBase.translation[i]) * w;

	float invBase[4];
	InverseQuat(origBase.rotation, invBase);
	float deltaRot[4];
	MultiplyQuat(invBase, rep.rotation, deltaRot);
	static constexpr float kIdentityQuat[4] = { 0.f, 0.f, 0.f, 1.f };
	float weightedDelta[4];
	SlerpQuat(kIdentityQuat, deltaRot, w, weightedDelta);
	MultiplyQuat(output.rotation, weightedDelta, output.rotation);
	// Composition drifts off unit length; this output is re-composed every frame
	// (and by every stacked additive filter), so renormalize at each step.
	NormalizeQuat(output.rotation);

	for (int i = 0; i < 3; ++i) {
		float deltaScale = (origBase.scale[i] > 0.0001f) ? (rep.scale[i] / origBase.scale[i]) : 1.0f;
		output.scale[i] *= (1.0f + (deltaScale - 1.0f) * w);
	}
}

// Resolve bone names in a TrackFilter to skeleton bone indices, against THIS character's
// skeleton (which may differ between 1p/3p bodies). Each character gets its own
// resolution so the same filter can apply correctly across all of an actor's bodies.
//
// Caller MUST hold s_trackFilterMutex (unique). The result is stored in a_resolved.
static void ResolveForChar(SubMod::TrackFilter* a_filter,
	CharResolved& a_resolved,
	RE::hkbCharacter* a_character)
{
	if (!a_filter || !a_character) return;

	uint64_t curVersion = a_filter->version.load(std::memory_order_relaxed);
	// version > 0 means we've resolved at least once at this version. Treat "no
	// matches" as a sticky cached negative result so we don't redo the search every
	// Generate call for skeletons that don't contain the wanted bones.
	if (a_resolved.version == curVersion) return;

	a_resolved.nameAndIndex.clear();
	a_resolved.freezeNameAndIndex.clear();
	a_resolved.version = curVersion;

	auto* setup = a_character->setup._ptr;
	if (!setup) return;

	auto* skeleton = reinterpret_cast<uint8_t*>(setup->animationSkeleton._ptr);
	if (!skeleton) return;

	auto* bonesArr = reinterpret_cast<RE::hkArrayRawLayout*>(skeleton + RE::kSkeletonOffset_bones);
	auto* parentArr = reinterpret_cast<RE::hkArrayRawLayout*>(skeleton + RE::kSkeletonOffset_parentIndices);
	if (!bonesArr->data || bonesArr->size <= 0) return;
	if (!parentArr->data || parentArr->size <= 0) return;

	int16_t numBones = static_cast<int16_t>(bonesArr->size);
	auto* boneData = reinterpret_cast<uint8_t*>(bonesArr->data);
	auto* parents = reinterpret_cast<int16_t*>(parentArr->data);

	std::vector<std::string> wantedNames;
	std::vector<std::string> excludeNames;
	std::vector<std::string> freezeNames;
	bool includeChildren;
	bool excludeChildren;
	{
		std::lock_guard lock(a_filter->boneMutex);
		wantedNames = a_filter->boneNames;
		includeChildren = a_filter->includeChildren;
		excludeNames = a_filter->excludeBoneNames;
		excludeChildren = a_filter->excludeChildren;
		freezeNames = a_filter->freezeBoneNames;
	}

	// Step 1: build the include set
	std::unordered_set<int16_t> matched;

	if (wantedNames.empty()) {
		// No explicit inclusion list — include every bone, then let exclusion subtract
		for (int16_t i = 0; i < numBones; ++i)
			matched.insert(i);
	} else {
		for (int16_t i = 0; i < numBones; ++i) {
			auto namePtr = *reinterpret_cast<uintptr_t*>(boneData + i * RE::kHkaBoneStride);
			namePtr &= ~uintptr_t(1);
			const char* boneName = reinterpret_cast<const char*>(namePtr);
			if (!boneName) continue;

			for (const auto& wanted : wantedNames) {
				if (_stricmp(boneName, wanted.c_str()) == 0) {
					matched.insert(i);
					break;
				}
			}
		}

		if (includeChildren && !matched.empty()) {
			bool changed = true;
			while (changed) {
				changed = false;
				for (int16_t i = 0; i < numBones; ++i) {
					if (matched.count(i)) continue;
					int16_t parentIdx = parents[i];
					if (parentIdx >= 0 && matched.count(parentIdx)) {
						matched.insert(i);
						changed = true;
					}
				}
			}
		}
	}

	// Step 2: build the exclude set and subtract from matched
	if (!excludeNames.empty() && !matched.empty()) {
		std::unordered_set<int16_t> excluded;
		for (int16_t i = 0; i < numBones; ++i) {
			auto namePtr = *reinterpret_cast<uintptr_t*>(boneData + i * RE::kHkaBoneStride);
			namePtr &= ~uintptr_t(1);
			const char* boneName = reinterpret_cast<const char*>(namePtr);
			if (!boneName) continue;

			for (const auto& excl : excludeNames) {
				if (_stricmp(boneName, excl.c_str()) == 0) {
					excluded.insert(i);
					break;
				}
			}
		}

		if (excludeChildren && !excluded.empty()) {
			bool changed = true;
			while (changed) {
				changed = false;
				for (int16_t i = 0; i < numBones; ++i) {
					if (excluded.count(i)) continue;
					int16_t parentIdx = parents[i];
					if (parentIdx >= 0 && excluded.count(parentIdx)) {
						excluded.insert(i);
						changed = true;
					}
				}
			}
		}

		for (int16_t idx : excluded) {
			matched.erase(idx);
		}
	}

	// Step 3: resolve the freeze set (freeze names + ALL their children - a
	// frozen weapon means the whole weapon subtree holds still) and subtract
	// it from the donor set: freeze wins over inclusion.
	std::unordered_set<int16_t> frozen;
	if (!freezeNames.empty()) {
		for (int16_t i = 0; i < numBones; ++i) {
			auto namePtr = *reinterpret_cast<uintptr_t*>(boneData + i * RE::kHkaBoneStride);
			namePtr &= ~uintptr_t(1);
			const char* boneName = reinterpret_cast<const char*>(namePtr);
			if (!boneName) continue;
			for (const auto& frz : freezeNames) {
				if (_stricmp(boneName, frz.c_str()) == 0) {
					frozen.insert(i);
					break;
				}
			}
		}
		if (!frozen.empty()) {
			bool changed = true;
			while (changed) {
				changed = false;
				for (int16_t i = 0; i < numBones; ++i) {
					if (frozen.count(i)) continue;
					int16_t parentIdx = parents[i];
					if (parentIdx >= 0 && frozen.count(parentIdx)) {
						frozen.insert(i);
						changed = true;
					}
				}
			}
		}
		for (int16_t idx : frozen) {
			matched.erase(idx);
		}
	}

	std::vector<int16_t> sortedIndices(matched.begin(), matched.end());
	std::sort(sortedIndices.begin(), sortedIndices.end());

	a_resolved.nameAndIndex.reserve(sortedIndices.size());
	for (int16_t idx : sortedIndices) {
		auto namePtr = *reinterpret_cast<uintptr_t*>(boneData + idx * RE::kHkaBoneStride);
		namePtr &= ~uintptr_t(1);
		const char* name = reinterpret_cast<const char*>(namePtr);
		if (name) {
			a_resolved.nameAndIndex.emplace_back(std::string(name), idx);
		}
	}

	std::vector<int16_t> sortedFrozen(frozen.begin(), frozen.end());
	std::sort(sortedFrozen.begin(), sortedFrozen.end());
	a_resolved.freezeNameAndIndex.reserve(sortedFrozen.size());
	for (int16_t idx : sortedFrozen) {
		auto namePtr = *reinterpret_cast<uintptr_t*>(boneData + idx * RE::kHkaBoneStride);
		namePtr &= ~uintptr_t(1);
		const char* name = reinterpret_cast<const char*>(namePtr);
		if (name) {
			a_resolved.freezeNameAndIndex.emplace_back(std::string(name), idx);
		}
	}

	logger::trace("[OAR-TrackFilter] Resolved {} bones on character {:X} (skel size={}, wanted={}, version={})",
		a_resolved.nameAndIndex.size(), reinterpret_cast<uintptr_t>(a_character),
		numBones, wantedNames.size(), curVersion);
	for (auto& [name, idx] : a_resolved.nameAndIndex) {
		logger::trace("[OAR-TrackFilter]   bone[{}] = '{}' (char {:X})",
			idx, name, reinterpret_cast<uintptr_t>(a_character));
	}

	// Diagnostic: dump bone names for every distinct character (one-shot per
	// character) so we can see exactly what skeletons exist for THIS actor.
	// Capped at 80 names to keep logs readable. Helps identify whether the
	// 1stperson body, a weapon sub-graph, etc. has a differently-named bone.
	{
		static std::unordered_set<RE::hkbCharacter*> s_dumped;
		if (s_dumped.insert(a_character).second) {
			logger::info("[OAR-TrackFilter-Dump] Char {:X} ({} bones) full skeleton dump:",
				reinterpret_cast<uintptr_t>(a_character), numBones);
			int dumpMax = std::min<int>(numBones, 80);
			for (int16_t i = 0; i < dumpMax; ++i) {
				auto namePtr = *reinterpret_cast<uintptr_t*>(boneData + i * RE::kHkaBoneStride);
				namePtr &= ~uintptr_t(1);
				const char* name = reinterpret_cast<const char*>(namePtr);
				logger::info("[OAR-TrackFilter-Dump]   bone[{}] = '{}'", i, name ? name : "(null)");
			}
		}
	}
}

void RegisterActorCharacter(RE::TESObjectREFR* a_refr)
{
	if (!a_refr) return;
	RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
	if (!a_refr->GetAnimationGraphManagerImpl(manager) || !manager) return;
	std::unique_lock lock(s_characterCacheMutex);
	for (uint32_t i = 0; i < manager->graph.size(); i++) {
		auto* character = &manager->graph[i]->character;
		s_characterCache[character] = a_refr;
		if (i == 0) {
			s_mainBodyCharacters.insert(character);
		}
	}
}

void ClearCharacterCache()
{
	std::unique_lock lock(s_characterCacheMutex);
	s_characterCache.clear();
	s_mainBodyCharacters.clear();
}

void PopulateKnownStringData()
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) return;

	RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
	if (!player->GetAnimationGraphManagerImpl(manager) || !manager) return;

	std::unique_lock lock(s_knownStringDataMutex);
	s_knownStringDataList.clear();

	logger::info("[OAR-StringData] Walking {} player graphs for stringData...", manager->graph.size());
	for (uint32_t i = 0; i < manager->graph.size(); i++) {
		auto* character = &manager->graph[i]->character;
		logger::info("[OAR-StringData]   graph[{}] character={:X} name='{}'", i,
			reinterpret_cast<uintptr_t>(character),
			(character->name.data() && !IsBadReadPtr(character->name.data(), 1)) ? character->name.data() : "(null)");

		auto* setup = character->setup._ptr;
		if (!setup || reinterpret_cast<uintptr_t>(setup) < 0x10000 || IsBadReadPtr(setup, 0x50)) {
			logger::info("[OAR-StringData]     setup=NULL/invalid ({:X})", reinterpret_cast<uintptr_t>(setup));
			continue;
		}
		logger::info("[OAR-StringData]     setup={:X}", reinterpret_cast<uintptr_t>(setup));

		auto* data = *reinterpret_cast<RE::hkbCharacterData**>(reinterpret_cast<uint8_t*>(setup) + 0x40);
		if (!data || reinterpret_cast<uintptr_t>(data) < 0x10000 || IsBadReadPtr(data, 0xC0)) {
			logger::info("[OAR-StringData]     data=NULL/invalid ({:X})", reinterpret_cast<uintptr_t>(data));
			continue;
		}
		logger::info("[OAR-StringData]     data={:X}", reinterpret_cast<uintptr_t>(data));

		auto* stringData = *reinterpret_cast<RE::hkbCharacterStringData**>(reinterpret_cast<uint8_t*>(data) + 0xB0);
		if (!stringData || reinterpret_cast<uintptr_t>(stringData) < 0x10000 || IsBadReadPtr(stringData, 0x40)) {
			logger::info("[OAR-StringData]     stringData=NULL/invalid ({:X})", reinterpret_cast<uintptr_t>(stringData));
			continue;
		}

		uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(stringData);
		logger::info("[OAR-StringData]     stringData={:X} vtbl={:X}", reinterpret_cast<uintptr_t>(stringData), vtbl);

		auto& animNames = stringData->animationNames;
		auto* arrBase = reinterpret_cast<const uint8_t*>(&animNames);
		auto* nameData = *reinterpret_cast<RE::hkbCharacterStringData::FileNameMeshNamePair* const*>(arrBase);
		int32_t nameSize = *reinterpret_cast<const int32_t*>(arrBase + 8);
		logger::info("[OAR-StringData]     animationNames: count={} data={:X}", nameSize, reinterpret_cast<uintptr_t>(nameData));

		if (nameData && !IsBadReadPtr(nameData, sizeof(void*)) && nameSize > 0) {
			int dumpCount = std::min(nameSize, 10);
			for (int j = 0; j < dumpCount; j++) {
				const char* fn = nameData[j].fileName.data();
				if (fn && reinterpret_cast<uintptr_t>(fn) > 0x10000 && !IsBadReadPtr(fn, 1)) {
					logger::info("[OAR-StringData]       [{}] '{}'", j, fn);
				}
			}
			if (nameSize > 10) {
				logger::info("[OAR-StringData]       ... ({} more entries)", nameSize - 10);
			}
		}

		s_knownStringDataList.push_back(stringData);
	}
	logger::info("[OAR-StringData] Collected {} known stringData objects", s_knownStringDataList.size());
}

// ============ Option 1: Read weapon graph's projectData->stringData->animationPath ============
// Accesses the REAL weapon hkbCharacter through PlayerCharacter's BSAnimationGraphManager,
// NOT through the broken a_context->character (which points to a static dummy character).
void RefreshWeaponAnimFolder()
{
	static std::atomic<int> s_refreshCallCount{ 0 };
	int callNum = s_refreshCallCount.fetch_add(1);
	bool verbose = (callNum < 3);

	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) return;

	RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
	if (!player->GetAnimationGraphManagerImpl(manager) || !manager) return;

	std::unique_lock lock(s_graphAnimPathMutex);
	s_graphAnimPathByIndex.clear();
	s_weaponAnimFolder.clear();
	s_weaponAnimFolderValid.store(false);

	for (uint32_t i = 0; i < manager->graph.size(); i++) {
		auto* character = &manager->graph[i]->character;
		if (!character || IsBadReadPtr(character, sizeof(void*))) continue;

		auto* projData = character->projectData._ptr;
		if (!projData || reinterpret_cast<uintptr_t>(projData) < 0x10000 ||
			IsBadReadPtr(projData, sizeof(RE::hkbProjectData))) {
			continue;
		}

		auto* projStrData = projData->stringData._ptr;
		if (!projStrData || reinterpret_cast<uintptr_t>(projStrData) < 0x10000 ||
			IsBadReadPtr(projStrData, sizeof(RE::hkbProjectStringData))) {
			continue;
		}

		const char* rawPath = projStrData->animationPath.data();
		std::string pathStr;
		if (rawPath && reinterpret_cast<uintptr_t>(rawPath) > 0x10000 &&
			!IsBadReadPtr(rawPath, 1) && rawPath[0] != '\0') {
			pathStr = rawPath;
		}

		if (verbose) {
			const char* charName = character->name.data();
			std::string nameStr = (charName && reinterpret_cast<uintptr_t>(charName) > 0x10000 &&
				!IsBadReadPtr(charName, 1)) ? charName : "(null)";
			logger::info("[OAR-WeaponPath]   graph[{}] name='{}' animationPath='{}'",
				i, nameStr, pathStr.empty() ? "(empty)" : pathStr);
		}

		if (!pathStr.empty()) {
			std::string normalized = pathStr;
			std::ranges::transform(normalized, normalized.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::ranges::replace(normalized, '/', '\\');

			while (!normalized.empty() && normalized.back() == '\\')
				normalized.pop_back();

			std::string folderPrefix;
			auto animPos = normalized.find("animations\\");
			if (animPos != std::string::npos) {
				folderPrefix = normalized.substr(animPos + 11);
			} else {
				auto lastSlash = normalized.rfind('\\');
				if (lastSlash != std::string::npos) {
					folderPrefix = normalized.substr(lastSlash + 1);
				} else {
					folderPrefix = normalized;
				}
			}

			if (!folderPrefix.empty()) {
				s_graphAnimPathByIndex[i] = folderPrefix;
				if (i >= 1) {
					s_weaponAnimFolder = folderPrefix;
					s_weaponAnimFolderValid.store(true);
					logger::info("[OAR-WeaponPath] Weapon animation folder = '{}'", folderPrefix);
				}
			}
		}
	}

	// Update subgraph ID -> folder mappings
	if (auto* actor = static_cast<RE::Actor*>(player)) {
		auto* process = actor->currentProcess;
		if (process && process->middleHigh) {
			auto* mh = process->middleHigh;

			auto& weapSubIDs = mh->currentWeaponSubGraphID;
			for (uint32_t idx = 0; idx < weapSubIDs.size(); idx++) {
				uint64_t id = weapSubIDs[idx].identifier;
				s_lastWeaponSubGraphID.store(id);

				if (s_weaponAnimFolderValid.load()) {
					std::unique_lock sgLock(s_subGraphToFolderMutex);
					if (s_subGraphIDToFolder.find(id) == s_subGraphIDToFolder.end()) {
						s_subGraphIDToFolder[id] = s_weaponAnimFolder;
						logger::info("[OAR-SubGraphID] Mapped {:X} -> '{}'", id, s_weaponAnimFolder);
					}
				}
			}
		}
	}

	if (verbose) {
		logger::info("[OAR-WeaponPath] Refresh complete. weaponFolder='{}' valid={}",
			s_weaponAnimFolder, s_weaponAnimFolderValid.load());
	}
}

namespace
{
	// Forward-declared — defined below, cleared from ClearClipRuntimeState
	static std::shared_mutex s_originalAnimMutex;
	static std::unordered_map<RE::hkbClipGenerator*, RE::hkaAnimation*> s_originalAnimMap;

	// Binding-identity memory for direct path matching: original GAME
	// hkaAnimation pointer -> the authoritatively resolved REAL path of the
	// file the engine actually loaded for that binding. Pooled/recycled
	// hkbClipGenerator instances all play the SAME underlying binding, but the
	// subgraph walk may succeed for one instance and fail for the next — and
	// the leaf-match fallback then GUESSES among all same-leaf candidates
	// (most-specific first). With a weapon that ships every attachment variant
	// on disk, that guess installed 'p890\fasthands\xmaglrg\wpnreload' for a
	// clip whose real file was 'p890\wpnreload' (2026-07-31 session log).
	// A known binding path is PROMOTED to a full authoritative resolution for
	// new clip instances (see the direct-path defer gate), so path matching —
	// not leaf guessing — stays in charge across clip pool recycling.
	// Learned only at Update/poll time (slot is current); never at Activate,
	// where a recycled clip's slot can still hold the previous clip's anim.
	static std::shared_mutex s_bindingSuffixMutex;
	static std::unordered_map<const RE::hkaAnimation*, std::string> s_bindingRealPath;

	// Save-load restore keyed by the BINDING SLOT rather than the clip.
	//
	// WHY: the hkbClipGenerator instances die on a mid-session save load
	// (kPreLoadGame's clip-keyed restore skipped ALL of them — "Skipped 61/175
	// dead/freed clip entries", 2026-08-01 log), but the animation BINDING the
	// clone was written into lives in the engine's animation DB cache and
	// SURVIVES the load. That is exactly how a retired clone leaked into the
	// NEW session's clips with no game original anywhere ([OAR-RecoveryFail]
	// spam + original-mod annotations playing after every reload-over-session).
	// Recording {slot -> clone written, game original} at swap time lets
	// RestoreAllActiveReplacements un-replace the surviving binding no matter
	// what happened to the clips: after the load the fresh clips see the real
	// game original, resolve normally, and rebuild fresh clones from it.
	//
	// Safety: a slot write happens ONLY when *slot still equals the exact clone
	// we recorded (an 8-byte match on freed/recycled binding memory is
	// negligible) AND the recorded original still carries a plausible game
	// animation vtable — the same guard level as the clip-keyed restore.
	struct BindingSlotBackup
	{
		RE::hkaAnimation* clone{ nullptr };
		RE::hkaAnimation* original{ nullptr };
	};
	static std::shared_mutex s_bindingSlotBackupMutex;
	static std::unordered_map<RE::hkaAnimation**, BindingSlotBackup> s_bindingSlotBackup;

	// Track the active SubMod per clip for firing custom "on end" events at deactivation.
	static std::shared_mutex s_activeSubModMutex;
	static std::unordered_map<RE::hkbClipGenerator*, SubMod*> s_activeSubModMap;
	// Binding original of the clip AT REGISTRATION TIME, same key and mutex.
	// The engine recycles clip generators without firing Deactivate, so an
	// entry can outlive the animation it was recorded for; readers that treat
	// the entry as a non-interruptible LOCK must compare this against the
	// clip's current binding and drop stale entries (ValidatedActiveSubMod) —
	// a stale lock skips condition evaluation and re-registered the old
	// weapon's submod onto the new weapon's clip (2026-08-16).
	static std::unordered_map<RE::hkbClipGenerator*, RE::hkaAnimation*> s_activeSubModBinding;

	// Clips whose triggers have been restored after the animation completed.
	// Prevents EnsureReplacementTriggersInstalled from re-NULLing them.
	static std::mutex s_triggersRestoredMutex;
	static std::unordered_set<RE::hkbClipGenerator*> s_triggersRestoredSet;

	// Direct Havok variable access (mirrors FPInertia approach).
	// Path: BSAnimationGraphManager → BShkbAnimationGraph (0xC0)
	//       → hkbCharacter (0x1C8) → hkbBehaviorGraph (0x80)
	//       → hkbVariableValueSet (0x110) → hkArray<hkbVariableValue>.m_data (0x10)
	static constexpr int kHavokVar_IsReloading = 31;

	static bool SetHavokBool(RE::Actor* a_actor, int a_index, bool a_val)
	{
		if (!a_actor) return false;
		RE::BSTSmartPointer<RE::BSAnimationGraphManager> mgr;
		if (!a_actor->GetAnimationGraphManagerImpl(mgr) || !mgr) return false;
		auto mgrAddr = reinterpret_cast<std::uintptr_t>(mgr.get());

		auto* graphPtr = *reinterpret_cast<void**>(mgrAddr + 0xC0);
		if (!graphPtr) return false;
		auto graphAddr = reinterpret_cast<std::uintptr_t>(graphPtr);

		auto* behaviorGraph = *reinterpret_cast<void**>(graphAddr + 0x1C8 + 0x80);
		if (!behaviorGraph) return false;
		auto bgAddr = reinterpret_cast<std::uintptr_t>(behaviorGraph);

		auto* varSet = *reinterpret_cast<void**>(bgAddr + 0x110);
		if (!varSet) return false;
		auto vsAddr = reinterpret_cast<std::uintptr_t>(varSet);

		struct VarArray { int32_t* data; int32_t size; int32_t capacityAndFlags; };
		auto* arr = reinterpret_cast<VarArray*>(vsAddr + 0x10);
		if (!arr->data || a_index < 0 || a_index >= arr->size) return false;

		arr->data[a_index] = a_val ? 1 : 0;
		return true;
	}

	// Validated access to s_originalAnimMap — returns nullptr and erases entry if the
	// stored pointer is freed/stale (IsBadReadPtr or vtable mismatch). This prevents
	// the crash scenario where weapon switch frees old animations but the map still
	// holds dangling pointers.
	// FO4 ships multiple hkaAnimation subclasses (spline-compressed, interleaved,
	// etc.), each with its own vtable in the game EXE. We capture ONE vtable for
	// packfile fixups (AnimationCache::SetVtableFromGame), but originals must
	// accept ANY plausible game-module vtable — exact-match-only previously
	// erased valid originals whose type differed from the first-seen one, so
	// GetOrBuildRuntimeAnim got a null template and never built (conditions
	// passed, track filter / swap silently no-op'd — e.g. 1911 Idle Empty).
	// Is this address inside the game EXE's actual load range? Replaces the old
	// "looks like a Windows-ASLR address" 0x7FF... range guess used by the
	// vtable sanity gates. That guess rejected EVERY vtable when the game loads
	// at its preferred base 0x140000000 — which is what Proton/Wine does, and
	// what ASLR-stripped exes used by RE folks do on Windows — so OAR built
	// replacements, played their sounds, but silently refused the final
	// animation swap (vanilla visuals + doubled audio, Proton field report,
	// 2026-08). hkaAnimation vtables always live in the game module (Havok is
	// statically linked), so exact module bounds are both correct and stricter
	// than the old heuristic: a stray heap pointer that happened to sit in the
	// 0x7FF range used to pass.
	static bool IsInGameModule(uintptr_t a_addr)
	{
		static const auto s_bounds = []() -> std::pair<uintptr_t, uintptr_t> {
			const auto base = reinterpret_cast<uintptr_t>(::GetModuleHandleW(nullptr));
			uintptr_t end = 0;
			if (base) {
				const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
				const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
				end = base + nt->OptionalHeader.SizeOfImage;
			}
			return { base, end };
		}();
		return a_addr >= s_bounds.first && a_addr < s_bounds.second;
	}

	static bool IsPlausibleGameAnimVtable(uintptr_t a_vtbl)
	{
		if (a_vtbl == 0) return false;
		auto expected = AnimationCache::GetSingleton()->GetGameAnimVtable();
		if (expected != 0 && a_vtbl == expected) return true;
		return IsInGameModule(a_vtbl);
	}

	static RE::hkaAnimation* GetValidOriginal(RE::hkbClipGenerator* a_clip)
	{
		std::shared_lock olock(s_originalAnimMutex);
		auto oit = s_originalAnimMap.find(a_clip);
		if (oit == s_originalAnimMap.end()) return nullptr;

		auto* candidate = oit->second;
		if (!candidate) {
			olock.unlock();
			std::unique_lock wlock(s_originalAnimMutex);
			s_originalAnimMap.erase(a_clip);
			return nullptr;
		}

		// Guard against freed memory — IsBadReadPtr returns TRUE if unreadable
		if (IsBadReadPtr(candidate, sizeof(uintptr_t))) {
			static int s_ibrLog = 0;
			if (s_ibrLog < 30) {
				logger::warn("[OAR-ValidOrig] originalAnim={:X} unreadable for clipGen={:X} — erasing",
					reinterpret_cast<uintptr_t>(candidate), reinterpret_cast<uintptr_t>(a_clip));
				s_ibrLog++;
			}
			olock.unlock();
			std::unique_lock wlock(s_originalAnimMutex);
			s_originalAnimMap.erase(a_clip);
			return nullptr;
		}

		auto vtbl = *reinterpret_cast<uintptr_t*>(candidate);
		if (!IsPlausibleGameAnimVtable(vtbl)) {
			static int s_vtblLog = 0;
			if (s_vtblLog < 30) {
				logger::warn("[OAR-ValidOrig] originalAnim={:X} vtbl={:X} not a game anim vtable for clipGen={:X} — erasing",
					reinterpret_cast<uintptr_t>(candidate), vtbl,
					reinterpret_cast<uintptr_t>(a_clip));
				s_vtblLog++;
			}
			olock.unlock();
			std::unique_lock wlock(s_originalAnimMutex);
			s_originalAnimMap.erase(a_clip);
			return nullptr;
		}

		return candidate;
	}

	// Cache clip suffixes from Activate (animationName may be cleared by Update time)
	static std::shared_mutex s_clipSuffixMutex;
	static std::unordered_map<RE::hkbClipGenerator*, std::string> s_clipSuffixCache;

	// Full on-disk animation path per clip, from the subgraph swap-array resolution
	// (Source S). Display-only: lets the Animation Log show the authoritative path
	// (e.g. "Actors\Character\_1stPerson\Animations\...") instead of just the suffix.
	static std::shared_mutex s_clipRealPathMutex;
	static std::unordered_map<RE::hkbClipGenerator*, std::string> s_clipRealPathCache;

	// Deferred subgraph resolution state. The Activate-time walk almost always
	// fails because clips activate exactly while the behavior graph is mid-
	// transition (stateOrTransitionChanged set, activeNodes rebuilding, nodeInfo
	// not yet assigned) — GunMover only ever walks the graph from a per-frame
	// hook OUTSIDE graph update. So we retry from the Update hook on subsequent
	// frames until the walk succeeds or the attempt budget runs out.
	//   - s_clipRealPathAuthoritative: clips whose cached path came from the
	//     subgraph walk (as opposed to the authored-name display fallback).
	//   - s_clipRealPathAttempts: per-clip frame counter for the direct-path
	//     defer gate in hkbClipGenerator_Update — while a player clip's real
	//     path is unresolved, replacement is held off until this counter
	//     exhausts its budget (then leaf matching applies as the fallback).
	//   - s_pendingActivateLog: kActivate anim-log entries held back until the
	//     path resolves (or attempts are exhausted), so the log shows the real
	//     on-disk path and correct 1st/3rd person tag instead of the authored
	//     relative name.
	struct PendingActivateLog
	{
		std::string suffix;
		uint64_t frame{ 0 };  // s_currentFrame at Activate time (for the flush grace period)
	};
	static std::shared_mutex s_clipRealPathStateMutex;
	static std::unordered_set<RE::hkbClipGenerator*> s_clipRealPathAuthoritative;
	static std::unordered_map<RE::hkbClipGenerator*, uint16_t> s_clipRealPathAttempts;
	static std::unordered_map<RE::hkbClipGenerator*, PendingActivateLog> s_pendingActivateLog;

	// Player clip ownership, accumulated by PollPlayerGraphClips() (GunMover's
	// GetAllClipInfo model: enumerate the PLAYER's graph manager's activeNodes,
	// so membership — not the hkbContext, whose character is a static dummy in
	// this runtime — decides which clips are the player's).
	// Value = index of the player root graph the clip was found in.
	//
	// STICKY: entries are inserted when the poll sees a clip and only removed
	// at Deactivate (or full state clear). The poll must skip a graph while it
	// rebuilds its node list — exactly when clips activate — so a per-frame
	// rebuild of this map would blank out membership at the moment the
	// deferred log flush needs it. Deactivate-erase keeps it from going stale.
	static std::shared_mutex s_playerClipMutex;
	static std::unordered_map<RE::hkbClipGenerator*, uint8_t> s_playerClipGraph;
	// Learned index of the player's 1st-person root graph: set when a clip from
	// that graph resolves to a "..._1stperson..." path. Lets us classify player
	// clips whose own paths lack the marker (authored relative names).
	static std::atomic<int32_t> s_firstPersonGraphIndex{ -1 };

	// Per-clip variant suffix cache (for kOnEachPlay: each clip gets its own roll)
	static std::shared_mutex s_clipVariantMutex;
	static std::unordered_map<RE::hkbClipGenerator*, std::string> s_clipVariantCache;

	// Per-clip flags for loop/echo events that allow non-interruptible submods to
	// re-evaluate conditions at specific points (matching Skyrim OAR behavior).
	static std::shared_mutex s_loopEchoFlagMutex;
	static std::unordered_map<RE::hkbClipGenerator*, bool> s_clipLoopPending;
	static std::unordered_map<RE::hkbClipGenerator*, bool> s_clipEchoPending;

	// Deactivation delay: per-clip timer that holds the replacement in place
	// for a configurable duration after conditions become false.
	struct DeactivationDelayState {
		float remaining{ 0.f };
		bool active{ false };
	};
	static std::shared_mutex s_deactDelayMutex;
	static std::unordered_map<RE::hkbClipGenerator*, DeactivationDelayState> s_deactivationDelay;

	// Manual annotation firing state — tracks localTime progression per clip
	struct ClipAnnotationState
	{
		float prevLocalTime{ -1.f };
		std::string activeSuffix;
		// The SubMod whose file's annotations are being tracked. Two SubMods can
		// register the SAME suffix with different files (per-file cache) — when
		// the winner flips mid-clip, lastFiredIndex would otherwise index into a
		// different annotation list.
		const void* activeOwner{ nullptr };
		int32_t lastFiredIndex{ -1 };
	};
	static std::shared_mutex s_annotStateMutex;
	static std::unordered_map<RE::hkbClipGenerator*, ClipAnnotationState> s_annotStateMap;

	// Track which suffixes currently have active replacements globally
	static std::shared_mutex s_activeReplacementSuffixMutex;
	static std::unordered_set<std::string> s_activeReplacementSuffixes;

	// Per-actor set of original-animation annotation strings to suppress while replacement active
	static std::shared_mutex s_origAnnotSetMutex;
	static std::unordered_map<uint32_t, std::unordered_set<std::string>> s_origAnnotByActor; // actorFormID -> set of annotation text

	// Per-actor active replacement set: actorFormID -> set of (clipGen, suffix) keys.
	// Suppression sink consults this to decide whether to suppress engine annotations.
	static std::shared_mutex s_activeReplacementActorMutex;
	static std::unordered_map<uint32_t, std::unordered_set<std::string>> s_activeReplacementByActor;

	// Backup of hkbClipGenerator::triggers/originalTriggers we replaced during swap, so we can
	// restore them when conditions stop matching. Without this, the engine's native annotation
	// processor fires the ORIGINAL animation's annotations (e.g., 44pistol sounds) regardless
	// of which hkaAnimation we swapped in — because triggers are keyed by binding, not anim.
	struct OARBuiltTriggerArray;  // defined below (REPLACEMENT TRIGGER BUILDER)
	struct TriggersBackup
	{
		void* triggers{ nullptr };          // raw hkRefPtr value (stored as void* — lifetime managed by Havok refcount)
		void* originalTriggers{ nullptr };
		bool nulled{ false };
		// The FILTERED trigger array we installed in place of the originals:
		// behavior-authored triggers kept (isAnnotation == false), annotation
		// triggers dropped (we fire the replacement's annotations manually).
		// Null when the original arrays carried no behavior triggers — then the
		// slots are NULL'd outright as before. Shared_ptr so retirement can
		// keep the memory alive after the backup entry is erased.
		std::shared_ptr<OARBuiltTriggerArray> filteredKeepAlive;
	};
	static std::shared_mutex s_triggersBackupMutex;
	static std::unordered_map<RE::hkbClipGenerator*, TriggersBackup> s_triggersBackup;

	static constexpr size_t kClipGenTriggersOffset = 0x98;
	static constexpr size_t kClipGenOriginalTriggersOffset = 0xD8;

	// =================== REPLACEMENT TRIGGER BUILDER ===================
	// Vtables resolved from REL::ID at init time (reliable — no dependency on encountering annotation triggers)
	static std::atomic<uintptr_t> s_vtableClipTriggerArray{ 0 };
	static std::atomic<uintptr_t> s_vtableStringEventPayload{ 0 };

	static void ResolveHavokVtables()
	{
		if (s_vtableClipTriggerArray.load() != 0 && s_vtableStringEventPayload.load() != 0)
			return;

		// REL::ID values from CommonLibF4 VTABLE_IDs.h
		REL::Relocation<uintptr_t> vtbl_ClipTriggerArray{ REL::ID(264032) };
		REL::Relocation<uintptr_t> vtbl_StringEventPayload{ REL::ID(1288131) };

		uintptr_t arrVtbl = vtbl_ClipTriggerArray.address();
		uintptr_t payVtbl = vtbl_StringEventPayload.address();

		s_vtableClipTriggerArray.store(arrVtbl);
		s_vtableStringEventPayload.store(payVtbl);

		logger::info("[OAR-TrigBuild] Resolved vtables from REL::ID — hkbClipTriggerArray={:X}, hkbStringEventPayload={:X}",
			arrVtbl, payVtbl);
	}

	// A built replacement trigger array that we manage ourselves.
	// All memory is heap-allocated and stable (no reallocation).
	struct OARBuiltTriggerArray
	{
		uint8_t* arrayHeader{ nullptr };        // 0x20 bytes: fake hkbClipTriggerArray
		uint8_t* triggerEntries{ nullptr };      // N * 0x20 bytes: array of hkbClipTrigger
		std::vector<uint8_t*> payloads;         // per-trigger hkbStringEventPayload (0x18 bytes each)
		std::vector<std::string> strings;       // keep strings alive (payloads point into these)

		RE::hkbClipTriggerArray* GetTriggerArray() const
		{
			return reinterpret_cast<RE::hkbClipTriggerArray*>(arrayHeader);
		}

		~OARBuiltTriggerArray()
		{
			delete[] arrayHeader;
			delete[] triggerEntries;
			for (auto* p : payloads) delete[] p;
		}
	};

	static std::shared_mutex s_builtTriggersMutex;
	static std::unordered_map<std::string, std::unique_ptr<OARBuiltTriggerArray>> s_builtTriggers; // suffix -> built array

	static RE::hkbClipTriggerArray* GetOrBuildReplacementTriggers(const std::string& a_suffix)
	{
		{
			std::shared_lock rlock(s_builtTriggersMutex);
			auto it = s_builtTriggers.find(a_suffix);
			if (it != s_builtTriggers.end() && it->second)
				return it->second->GetTriggerArray();
		}

		uintptr_t arrVtbl = s_vtableClipTriggerArray.load();
		uintptr_t payVtbl = s_vtableStringEventPayload.load();
		if (!arrVtbl || !payVtbl) return nullptr;

		auto* cache = AnimationCache::GetSingleton();
		auto* annotations = cache->GetAnnotations(a_suffix);
		if (!annotations || annotations->empty()) return nullptr;

		const size_t trigCount = annotations->size();
		const size_t kTriggerSize = 0x20;
		const size_t kArrayHeaderSize = 0x20;
		const size_t kPayloadSize = 0x18;

		auto built = std::make_unique<OARBuiltTriggerArray>();
		built->strings.resize(trigCount);
		built->payloads.resize(trigCount);

		built->triggerEntries = new uint8_t[trigCount * kTriggerSize]();
		built->arrayHeader = new uint8_t[kArrayHeaderSize]();

		for (size_t i = 0; i < trigCount; ++i) {
			auto& annot = (*annotations)[i];
			built->strings[i] = annot.text;

			auto* pMem = new uint8_t[kPayloadSize]();
			built->payloads[i] = pMem;

			*reinterpret_cast<uintptr_t*>(pMem + 0x00) = payVtbl;
			*reinterpret_cast<uint32_t*>(pMem + 0x08) = 0x80000000u | 0x7FFF;
			*reinterpret_cast<const char**>(pMem + 0x10) = built->strings[i].c_str();

			uint8_t* tMem = built->triggerEntries + i * kTriggerSize;
			*reinterpret_cast<float*>(tMem + 0x00) = annot.time;
			*reinterpret_cast<int32_t*>(tMem + 0x08) = -1;
			*reinterpret_cast<RE::hkbEventPayload**>(tMem + 0x10) = reinterpret_cast<RE::hkbEventPayload*>(pMem);
			tMem[0x18] = 0;
			tMem[0x19] = 0;
			tMem[0x1A] = 1;
		}

		uint8_t* aMem = built->arrayHeader;
		*reinterpret_cast<uintptr_t*>(aMem + 0x00) = arrVtbl;
		*reinterpret_cast<uint32_t*>(aMem + 0x08) = 0x80000000u | 0x7FFF;
		*reinterpret_cast<uint8_t**>(aMem + 0x10) = built->triggerEntries;
		*reinterpret_cast<int32_t*>(aMem + 0x18) = static_cast<int32_t>(trigCount);
		*reinterpret_cast<uint32_t*>(aMem + 0x1C) = static_cast<uint32_t>(trigCount) | 0x80000000u;

		logger::info("[OAR-TrigBuild] Built replacement triggers for '{}': {} entries", a_suffix, trigCount);
		for (size_t i = 0; i < trigCount && i < 5; ++i) {
			logger::info("[OAR-TrigBuild]   t={:.4f}s '{}'", (*annotations)[i].time, (*annotations)[i].text);
		}

		auto* result = built->GetTriggerArray();
		std::unique_lock wlock(s_builtTriggersMutex);
		s_builtTriggers[a_suffix] = std::move(built);
		return result;
	}

	// Keep-alive for filtered trigger arrays that were replaced/abandoned while
	// a clip might still reference them (same pattern as the retired animation
	// clones). Capped; by the time the oldest half is dropped, no clip from
	// that era can still be active.
	static std::mutex s_retiredTrigMutex;
	static std::vector<std::shared_ptr<OARBuiltTriggerArray>> s_retiredFilteredTriggers;

	static void RetireFilteredTriggers(std::shared_ptr<OARBuiltTriggerArray>&& a_arr)
	{
		if (!a_arr) return;
		std::lock_guard g(s_retiredTrigMutex);
		if (s_retiredFilteredTriggers.size() >= 256) {
			s_retiredFilteredTriggers.erase(
				s_retiredFilteredTriggers.begin(),
				s_retiredFilteredTriggers.begin() + 128);
		}
		s_retiredFilteredTriggers.push_back(std::move(a_arr));
	}

	// Collect the annotation text strings of an hkaAnimation (raw guarded parse,
	// same offsets as the Activate-time original-annotation cache). Used to
	// recognize annotation-derived triggers even if the isAnnotation flag is
	// not reliably set by this runtime's loader.
	static std::unordered_set<std::string> CollectAnimAnnotationTexts(RE::hkaAnimation* a_anim)
	{
		std::unordered_set<std::string> result;
		if (!a_anim || IsBadReadPtr(a_anim, 0x40)) return result;

		auto* bytes = reinterpret_cast<uint8_t*>(a_anim);
		auto* trackPtr = *reinterpret_cast<uint8_t**>(bytes + 0x28);
		int32_t trackCount = *reinterpret_cast<int32_t*>(bytes + 0x30);
		if (!trackPtr || trackCount <= 0 || trackCount > 0x200 ||
			reinterpret_cast<uintptr_t>(trackPtr) < 0x10000 ||
			IsBadReadPtr(trackPtr, static_cast<size_t>(trackCount) * 0x18)) {
			return result;
		}

		for (int32_t t = 0; t < trackCount; ++t) {
			auto* trackBase = trackPtr + (t * 0x18);
			auto* annots = *reinterpret_cast<uint8_t**>(trackBase + 0x08);
			int32_t annotCount = *reinterpret_cast<int32_t*>(trackBase + 0x10);
			if (!annots || annotCount <= 0 || annotCount > 0x1000 ||
				reinterpret_cast<uintptr_t>(annots) < 0x10000 ||
				IsBadReadPtr(annots, static_cast<size_t>(annotCount) * 0x10)) {
				continue;
			}
			for (int32_t a = 0; a < annotCount; ++a) {
				auto rawTxt = *reinterpret_cast<uintptr_t*>(annots + a * 0x10 + 0x08) & ~uintptr_t(1);
				auto* txt = reinterpret_cast<const char*>(rawTxt);
				if (txt && rawTxt > 0x10000 && !IsBadReadPtr(txt, 1) && txt[0] != '\0') {
					result.insert(txt);
				}
			}
		}
		return result;
	}

	// Read a trigger's event text from its payload, when the payload is an
	// hkbStringEventPayload (string at +0x10). Returns nullptr otherwise.
	static const char* TriggerPayloadText(const uint8_t* a_trigger)
	{
		auto* payload = *reinterpret_cast<uint8_t* const*>(a_trigger + 0x10);
		if (!payload || reinterpret_cast<uintptr_t>(payload) < 0x10000 ||
			IsBadReadPtr(payload, 0x18)) {
			return nullptr;
		}
		// Only trust +0x10 as a string when the vtable says StringEventPayload.
		const auto payVtbl = s_vtableStringEventPayload.load();
		if (payVtbl == 0 || *reinterpret_cast<const uintptr_t*>(payload) != payVtbl) {
			return nullptr;
		}
		auto rawTxt = *reinterpret_cast<const uintptr_t*>(payload + 0x10) & ~uintptr_t(1);
		auto* txt = reinterpret_cast<const char*>(rawTxt);
		if (!txt || rawTxt < 0x10000 || IsBadReadPtr(txt, 1) || txt[0] == '\0') {
			return nullptr;
		}
		return txt;
	}

	// Build a filtered copy of a live hkbClipTriggerArray that KEEPS the
	// behavior-authored triggers and DROPS the annotation-derived ones.
	//
	// WHY: the trigger array carries two kinds of entries. Annotation triggers
	// (isAnnotation == true) are built from the animation's annotation tracks —
	// sounds, WeaponFire — and must be muted during replacement (OAR fires the
	// REPLACEMENT's annotations manually). Behavior triggers (isAnnotation ==
	// false) come from the behavior graph itself and drive the state machine
	// AND engine-side actions: on equip clips, the weapon attach/draw event and
	// the equip-complete transition. The previous implementation NULL'd the
	// whole array, which silenced those too — replacing wpnequip made the
	// weapon invisible and the graph fall back to the fast-equip path even
	// with a byte-identical replacement file.
	//
	// A trigger is treated as annotation-derived when EITHER its isAnnotation
	// flag is set OR its payload text matches one of the original animation's
	// annotation texts (belt-and-suspenders in case this runtime's loader
	// doesn't set the flag).
	//
	// Returns nullptr when the source has no behavior triggers to keep — the
	// caller then installs NULL, exactly the old behavior.
	//
	// FUTURE REDESIGN HOOK: this is where replacement annotations could be
	// appended as native trigger entries (times from the replacement file,
	// event IDs/payload layout cloned from the original's annotation triggers)
	// so the ENGINE fires them instead of our manual replay in the Update hook.
	// See the "FUTURE REDESIGN NOTE" above the annotation-firing block in
	// hkbClipGenerator_Update for the full rationale.
	// a_endClipDuration >= 0: "End Clip If Shorter" is active and this is the
	// REPLACEMENT's duration. Kept triggers flagged relativeToEndOfClip store a
	// negative offset from the clip's end and are fired by the engine at
	// (clipDuration + t) — with clipDuration derived from the ORIGINAL, which is
	// exactly why a shorter replacement's clip kept running to the original's
	// end (the state-machine transition events fired on the original timeline;
	// hazord606 wpnmelee: transitions at 2.233s while the donor ended at
	// 1.767s). Rewriting them to ABSOLUTE times against the replacement's
	// duration makes the engine fire its own transition events at the
	// replacement's end — the clip genuinely ends there, no held frame.
	// (An earlier attempt wrote syncInfo->duration instead; the engine
	// recomputes that every frame, so it was a no-op.)
	// a_keepAnnotations: keep annotation-derived triggers too (used when the
	// submod does NOT replace annotations but still needs the re-timed copy for
	// End Clip If Shorter — the native annotations must keep firing).
	static std::shared_ptr<OARBuiltTriggerArray> BuildBehaviorOnlyTriggers(
		void* a_src, const std::unordered_set<std::string>& a_origAnnotTexts,
		float a_endClipDuration = -1.0f, bool a_keepAnnotations = false)
	{
		if (!a_src || reinterpret_cast<uintptr_t>(a_src) < 0x10000 ||
			IsBadReadPtr(a_src, 0x20)) {
			return nullptr;
		}

		constexpr size_t kTriggerSize = 0x20;
		auto* srcBytes = reinterpret_cast<uint8_t*>(a_src);
		auto* srcData = *reinterpret_cast<uint8_t**>(srcBytes + 0x10);
		const int32_t srcCount = *reinterpret_cast<int32_t*>(srcBytes + 0x18);
		if (!srcData || srcCount <= 0 || srcCount > 0x400 ||
			reinterpret_cast<uintptr_t>(srcData) < 0x10000 ||
			IsBadReadPtr(srcData, static_cast<size_t>(srcCount) * kTriggerSize)) {
			return nullptr;
		}

		static int s_dumpLog = 0;
		const bool dump = (s_dumpLog < 8);
		if (dump) s_dumpLog++;

		std::vector<int32_t> keep;
		keep.reserve(static_cast<size_t>(srcCount));
		for (int32_t i = 0; i < srcCount; ++i) {
			const uint8_t* trig = srcData + i * kTriggerSize;
			const bool isAnnotFlag = trig[0x1A] != 0;  // hkbClipTrigger::isAnnotation
			const char* text = TriggerPayloadText(trig);
			const bool matchesOrigAnnot = text && a_origAnnotTexts.count(text) > 0;
			const bool drop = !a_keepAnnotations && (isAnnotFlag || matchesOrigAnnot);
			if (!drop) keep.push_back(i);
			if (dump) {
				logger::info("[OAR-TrigFilter]   [{}] t={:.3f} id={} isAnnot={} text='{}' -> {}",
					i, *reinterpret_cast<const float*>(trig),
					*reinterpret_cast<const int32_t*>(trig + 0x08),
					isAnnotFlag, text ? text : "(none)", drop ? "DROP" : "KEEP");
			}
		}
		if (dump) {
			logger::info("[OAR-TrigFilter] src={:X}: {} triggers, kept {} behavior triggers",
				reinterpret_cast<uintptr_t>(a_src), srcCount, keep.size());
		}
		if (keep.empty()) return nullptr;

		// Vtable: prefer the REL::ID-resolved one; fall back to the source's own.
		uintptr_t arrVtbl = s_vtableClipTriggerArray.load();
		if (!arrVtbl) arrVtbl = *reinterpret_cast<uintptr_t*>(a_src);

		auto built = std::make_shared<OARBuiltTriggerArray>();
		built->triggerEntries = new uint8_t[keep.size() * kTriggerSize]();
		built->arrayHeader = new uint8_t[0x20]();

		// Shallow-copy the kept triggers. Their payload pointers stay valid:
		// the backup holds the reference the clip previously owned on the
		// source array (we never decrement it), so the source and its payloads
		// outlive this filtered copy.
		for (size_t k = 0; k < keep.size(); ++k) {
			std::memcpy(built->triggerEntries + k * kTriggerSize,
				srcData + static_cast<size_t>(keep[k]) * kTriggerSize, kTriggerSize);
		}

		// End Clip If Shorter: re-time relative-to-end triggers against the
		// replacement's duration (see the function comment). Only our private
		// copy is touched — the source array is untouched and restored at the
		// end of the play as usual.
		if (a_endClipDuration > 0.0f) {
			for (size_t k = 0; k < keep.size(); ++k) {
				uint8_t* trig = built->triggerEntries + k * kTriggerSize;
				if (trig[0x18] != 0) {  // hkbClipTrigger::relativeToEndOfClip
					const float rel = *reinterpret_cast<float*>(trig);
					float absTime = a_endClipDuration + rel;  // rel is negative
					if (absTime < 0.0f) absTime = 0.0f;
					*reinterpret_cast<float*>(trig) = absTime;
					trig[0x18] = 0;
					static int s_retimeLog = 0;
					if (s_retimeLog < 30) {
						logger::info("[OAR-TrigFilter] EndClipIfShorter: re-timed end trigger id={} rel={:.3f} -> abs={:.3f} (repDur={:.3f})",
							*reinterpret_cast<const int32_t*>(trig + 0x08), rel, absTime, a_endClipDuration);
						s_retimeLog++;
					}
				}
			}
		}

		uint8_t* aMem = built->arrayHeader;
		*reinterpret_cast<uintptr_t*>(aMem + 0x00) = arrVtbl;
		// refCount/memSize pattern: huge refcount + "not heap-owned" so the
		// engine can addRef/release freely without ever deallocating our memory.
		*reinterpret_cast<uint32_t*>(aMem + 0x08) = 0x80000000u | 0x7FFF;
		*reinterpret_cast<uint8_t**>(aMem + 0x10) = built->triggerEntries;
		*reinterpret_cast<int32_t*>(aMem + 0x18) = static_cast<int32_t>(keep.size());
		*reinterpret_cast<uint32_t*>(aMem + 0x1C) = static_cast<uint32_t>(keep.size()) | 0x80000000u;
		return built;
	}

	// The original animation for annotation-text matching: the validated map
	// entry when present, else the animation currently in the slot as long as
	// it is not our replacement.
	static RE::hkaAnimation* OriginalAnimForTriggerFilter(RE::hkbClipGenerator* a_clipGen)
	{
		if (auto* orig = GetValidOriginal(a_clipGen)) return orig;
		auto** slot = a_clipGen->GetAnimationSlot();
		if (slot && *slot && !AnimationCache::GetSingleton()->IsOurReplacement(*slot)) {
			return *slot;
		}
		return nullptr;
	}

	// ===== Vanilla annotation backup ==========================================
	// Safety net for UN-replaced plays that start while engine-side trigger
	// state was built against a stale clone. Field-proven failure (MP7A2
	// 2026-08-19, three builds): the empty reload decides correctly at t=0.000
	// and plays the vanilla animation, yet its tail sounds never fire — the
	// play-local trigger data was constructed at _Activate from the 2.292s
	// clone still parked in the shared binding (9-of-12 array in the field
	// log), and no post-activation slot restore can bring back triggers the
	// engine never created for the play. Instead of depending on any engine
	// theory, this diffs the ORIGINAL animation's annotation tracks against
	// the clip's LIVE trigger array by time and manually fires only the
	// MISSING ones at their authored times — the same dispatch the replacement
	// annotation path uses on every tactical reload. On a healthy play nothing
	// is missing and nothing is armed; double-firing is impossible by
	// construction. Early state exits are covered by the Deactivate-side
	// flush (same end-window rule as FlushPendingEndAnnotations).
	// Defined later in this file; needed by the flush below.
	static bool PlaySoundDirect(const char* a_soundName, RE::TESObjectREFR* a_refr);
	static void QueueCustomEvents(RE::TESObjectREFR* a_refr, const std::vector<std::string>& a_events, const char* a_label);

	struct VanillaAnnotEntry
	{
		float time;
		std::string text;
	};
	struct VanillaAnnotBackup
	{
		std::vector<VanillaAnnotEntry> entries;  // sorted by time; only the missing ones
		float prevT{ 0.f };
		int32_t lastFired{ -1 };
		float origDuration{ 0.f };
	};
	static std::shared_mutex s_vanillaAnnotMutex;
	static std::unordered_map<RE::hkbClipGenerator*, VanillaAnnotBackup> s_vanillaAnnotMap;
	static std::atomic<int> s_vanillaAnnotCount{ 0 };  // cheap empty check for the hot Update path

	// Per-play integrity-check bookkeeping: last localTime seen per clip; a
	// regression = new play on the same generator = re-run the check.
	static std::mutex s_annotIntegrityMutex;
	static std::unordered_map<RE::hkbClipGenerator*, float> s_annotIntegrityLastT;

	// Enumerate (time, text) annotation pairs from a game hkaAnimation's raw
	// annotation tracks — same guarded offsets as CollectAnimAnnotationTexts.
	static void CollectAnimAnnotationsTimed(RE::hkaAnimation* a_anim,
		std::vector<VanillaAnnotEntry>& a_out)
	{
		a_out.clear();
		if (!a_anim || IsBadReadPtr(a_anim, 0x40)) return;
		auto* bytes = reinterpret_cast<uint8_t*>(a_anim);
		auto* trackPtr = *reinterpret_cast<uint8_t**>(bytes + 0x28);
		int32_t trackCount = *reinterpret_cast<int32_t*>(bytes + 0x30);
		if (!trackPtr || trackCount <= 0 || trackCount > 0x200 ||
			reinterpret_cast<uintptr_t>(trackPtr) < 0x10000 ||
			IsBadReadPtr(trackPtr, static_cast<size_t>(trackCount) * 0x18)) {
			return;
		}
		for (int32_t t = 0; t < trackCount; ++t) {
			auto* trackBase = trackPtr + (t * 0x18);
			auto* annots = *reinterpret_cast<uint8_t**>(trackBase + 0x08);
			int32_t annotCount = *reinterpret_cast<int32_t*>(trackBase + 0x10);
			if (!annots || annotCount <= 0 || annotCount > 0x1000 ||
				reinterpret_cast<uintptr_t>(annots) < 0x10000 ||
				IsBadReadPtr(annots, static_cast<size_t>(annotCount) * 0x10)) {
				continue;
			}
			for (int32_t a = 0; a < annotCount; ++a) {
				const float time = *reinterpret_cast<float*>(annots + a * 0x10 + 0x00);
				auto rawTxt = *reinterpret_cast<uintptr_t*>(annots + a * 0x10 + 0x08) & ~uintptr_t(1);
				auto* txt = reinterpret_cast<const char*>(rawTxt);
				if (txt && rawTxt > 0x10000 && !IsBadReadPtr(txt, 1) && txt[0] != '\0') {
					a_out.push_back({ time, txt });
				}
			}
		}
		std::sort(a_out.begin(), a_out.end(),
			[](const VanillaAnnotEntry& a, const VanillaAnnotEntry& b) { return a.time < b.time; });
	}

	// Arm the backup for a play whose annotations must come from the given
	// animation's tracks: diff them against the clip's live trigger array and
	// register only the missing ones. Matching is time (20ms epsilon) PLUS
	// text: two annotations sharing a timestamp cannot mask each other. An
	// annotation's trigger stores its text with the "SoundPlay."/"CullBone."
	// style prefix stripped, so the annotation text is compared both whole and
	// as ".<triggerText>" suffix; a trigger without readable text (id-only
	// payload, e.g. reloadComplete) covers any annotation at its time — that
	// asymmetry prevents double-fires on id-converted triggers at the cost of
	// one vanishingly-rare miss (two same-time annotations where exactly the
	// untexted one survived). a_startTime = the clip's current localTime so a
	// mid-play arm never replays earlier annotations.
	static void ArmVanillaAnnotationBackup(RE::hkbClipGenerator* a_clip,
		RE::hkaAnimation* a_original, float a_startTime)
	{
		if (!a_clip || !a_original) return;

		std::vector<VanillaAnnotEntry> origAnnots;
		CollectAnimAnnotationsTimed(a_original, origAnnots);
		if (origAnnots.empty()) return;

		// Live triggers (time + payload text) from the clip's current array.
		std::vector<std::pair<float, const char*>> liveTrigs;
		int32_t liveCount = 0;
		{
			auto* bytes = reinterpret_cast<uint8_t*>(a_clip);
			auto* trigArr = *reinterpret_cast<uint8_t**>(bytes + kClipGenTriggersOffset);
			if (trigArr && reinterpret_cast<uintptr_t>(trigArr) > 0x10000 &&
				!IsBadReadPtr(trigArr, 0x20)) {
				constexpr size_t kTriggerSize = 0x20;
				auto* data = *reinterpret_cast<uint8_t**>(trigArr + 0x10);
				liveCount = *reinterpret_cast<int32_t*>(trigArr + 0x18);
				if (data && liveCount > 0 && liveCount <= 0x400 &&
					reinterpret_cast<uintptr_t>(data) > 0x10000 &&
					!IsBadReadPtr(data, static_cast<size_t>(liveCount) * kTriggerSize)) {
					liveTrigs.reserve(static_cast<size_t>(liveCount));
					for (int32_t i = 0; i < liveCount; ++i) {
						const uint8_t* trig = data + i * kTriggerSize;
						liveTrigs.emplace_back(
							*reinterpret_cast<const float*>(trig),
							TriggerPayloadText(trig));
					}
				} else {
					liveCount = 0;
				}
			}
		}

		auto annotCoveredBy = [](const std::string& a_annot, const char* a_trigText) -> bool {
			if (!a_trigText || a_trigText[0] == '\0') return true;  // untexted trigger at the time covers
			if (_stricmp(a_annot.c_str(), a_trigText) == 0) return true;
			const size_t tLen = std::strlen(a_trigText);
			if (a_annot.size() > tLen + 1) {
				const size_t off = a_annot.size() - tLen;
				if (a_annot[off - 1] == '.' &&
					_stricmp(a_annot.c_str() + off, a_trigText) == 0) {
					return true;
				}
			}
			return false;
		};

		VanillaAnnotBackup backup;
		for (auto& ann : origAnnots) {
			bool covered = false;
			for (auto& [lt, ltext] : liveTrigs) {
				if (std::abs(lt - ann.time) <= 0.02f && annotCoveredBy(ann.text, ltext)) {
					covered = true;
					break;
				}
			}
			if (!covered) backup.entries.push_back(std::move(ann));
		}

		static std::atomic<int> s_armLog{ 0 };
		if (backup.entries.empty()) {
			if (s_armLog.fetch_add(1, std::memory_order_relaxed) < 30) {
				logger::info("[OAR-VanillaBackup] engine trigger array complete for clipGen={:X} ({} triggers, {} annots) — no backup needed",
					reinterpret_cast<uintptr_t>(a_clip), liveCount, origAnnots.size());
			}
			return;
		}

		backup.prevT = a_startTime;
		backup.lastFired = -1;
		// Skip entries already behind the arm point (mid-play arm).
		for (size_t i = 0; i < backup.entries.size(); ++i) {
			if (backup.entries[i].time <= a_startTime) {
				backup.lastFired = static_cast<int32_t>(i);
			} else {
				break;
			}
		}
		if (!IsBadReadPtr(a_original, 0x18)) {
			backup.origDuration = *reinterpret_cast<const float*>(
				reinterpret_cast<const uint8_t*>(a_original) + 0x14);
		}

		if (s_armLog.fetch_add(1, std::memory_order_relaxed) < 30) {
			std::string names;
			for (auto& e : backup.entries) {
				if (!names.empty()) names += ", ";
				names += std::format("'{}'@{:.3f}", e.text, e.time);
			}
			logger::info("[OAR-VanillaBackup] armed {} missing annotation(s) for clipGen={:X} (live triggers={}, original annots={}, from t={:.3f}): {}",
				backup.entries.size(), reinterpret_cast<uintptr_t>(a_clip),
				liveCount, origAnnots.size(), a_startTime, names);
		}

		{
			std::unique_lock lock(s_vanillaAnnotMutex);
			auto [it, inserted] = s_vanillaAnnotMap.insert_or_assign(a_clip, std::move(backup));
			if (inserted) s_vanillaAnnotCount.fetch_add(1, std::memory_order_relaxed);
		}
	}

	// Fire and erase whatever remains for a clip whose play is ending. Same
	// end-window rule as FlushPendingEndAnnotations: only flush when tracking
	// got within 1.0s of the original's end (a genuine early cancel flushes
	// nothing). Sounds fire immediately; graph events go through the deferred
	// queue.
	static void FlushVanillaAnnotBackup(RE::hkbClipGenerator* a_clip, RE::TESObjectREFR* a_refr, const char* a_reason)
	{
		VanillaAnnotBackup backup;
		{
			std::unique_lock lock(s_vanillaAnnotMutex);
			auto it = s_vanillaAnnotMap.find(a_clip);
			if (it == s_vanillaAnnotMap.end()) return;
			backup = std::move(it->second);
			s_vanillaAnnotMap.erase(it);
			s_vanillaAnnotCount.fetch_sub(1, std::memory_order_relaxed);
		}
		const int32_t total = static_cast<int32_t>(backup.entries.size());
		if (backup.lastFired + 1 >= total) return;
		if (backup.origDuration <= 0.01f ||
			backup.prevT < backup.origDuration - 1.0f) {
			return;
		}
		auto* refr = a_refr;
		if (!refr) refr = RE::PlayerCharacter::GetSingleton();
		std::vector<std::string> events;
		for (int32_t i = backup.lastFired + 1; i < total; ++i) {
			const auto& e = backup.entries[i];
			static constexpr const char* kSoundPlayPrefix = "SoundPlay.";
			if (e.text.size() > 10 && _strnicmp(e.text.c_str(), kSoundPlayPrefix, 10) == 0) {
				if (refr) PlaySoundDirect(e.text.c_str() + 10, refr);
			} else {
				events.push_back(e.text);
			}
			static std::atomic<int> s_vbFlushLog{ 0 };
			if (s_vbFlushLog.fetch_add(1, std::memory_order_relaxed) < 60) {
				logger::info("[OAR-VanillaBackup] End-flush '{}' (clipGen={:X}, {}, prevT={:.3f})",
					e.text, reinterpret_cast<uintptr_t>(a_clip), a_reason, backup.prevT);
			}
		}
		if (refr && !events.empty()) {
			QueueCustomEvents(refr, events, "vanilla-backup end-flush");
		}
	}

	// Effective "End Clip If Shorter" duration for a play: the replacement's
	// duration when the submod has the toggle on, the clip is a one-shot, and
	// the replacement is meaningfully shorter than the original. -1 = no
	// re-timing (the default trigger behavior).
	static float EndClipIfShorterDuration(const SubMod* a_subMod, RE::hkbClipGenerator* a_clip,
		RE::hkaAnimation* a_replacement, RE::hkaAnimation* a_original)
	{
		if (!a_subMod || !a_subMod->GetEndClipIfShorter() || !a_clip || !a_replacement) return -1.0f;
		if (a_clip->mode == RE::MODE_LOOPING) return -1.0f;
		if (IsBadReadPtr(a_replacement, 0x18)) return -1.0f;
		if (!a_original || IsBadReadPtr(a_original, 0x18)) return -1.0f;
		const float repDur = *reinterpret_cast<const float*>(
			reinterpret_cast<const uint8_t*>(a_replacement) + 0x14);
		const float origDur = *reinterpret_cast<const float*>(
			reinterpret_cast<const uint8_t*>(a_original) + 0x14);
		return (repDur > 0.01f && origDur > repDur + 0.02f) ? repDur : -1.0f;
	}

	// Replace the clip generator's triggers with a filtered array that keeps
	// behavior-authored triggers but drops annotation-derived ones (see
	// BuildBehaviorOnlyTriggers). The replacement's annotations are fired
	// manually via the dual-path emission system below; behavior triggers
	// (weapon attach on equip, state-machine transitions) keep firing natively.
	static void InstallReplacementTriggers(RE::hkbClipGenerator* a_clipGen, const std::string& /*a_replacementSuffix*/,
		float a_endClipDuration = -1.0f, bool a_keepAnnotations = false)
	{
		if (!a_clipGen) return;
		auto* bytes = reinterpret_cast<uint8_t*>(a_clipGen);
		auto* triggersPtr = reinterpret_cast<void**>(bytes + kClipGenTriggersOffset);
		auto* origTriggersPtr = reinterpret_cast<void**>(bytes + kClipGenOriginalTriggersOffset);

		std::unique_lock lock(s_triggersBackupMutex);
		auto& backup = s_triggersBackup[a_clipGen];
		if (!backup.nulled) {
			backup.triggers = *triggersPtr;
			backup.originalTriggers = *origTriggersPtr;
			backup.nulled = true;

			// OAR now owns this play's annotations (manual replacement firing,
			// or an ECIS-re-timed native copy): disarm any vanilla backup the
			// per-play integrity check registered — its "original annotations
			// at original times" contract no longer applies to this play.
			{
				std::unique_lock vbLock(s_vanillaAnnotMutex);
				if (s_vanillaAnnotMap.erase(a_clipGen)) {
					s_vanillaAnnotCount.fetch_sub(1, std::memory_order_relaxed);
				}
			}

			const auto origAnnotTexts = CollectAnimAnnotationTexts(OriginalAnimForTriggerFilter(a_clipGen));
			backup.filteredKeepAlive = BuildBehaviorOnlyTriggers(
				backup.triggers ? backup.triggers : backup.originalTriggers, origAnnotTexts,
				a_endClipDuration, a_keepAnnotations);

			static int s_installLog = 0;
			if (s_installLog < 30) {
				logger::info("[OAR-Triggers] Filtered clipGen={:X} orig triggers={:X}/{:X} -> behaviorOnly={:X}",
					reinterpret_cast<uintptr_t>(a_clipGen),
					reinterpret_cast<uintptr_t>(backup.triggers),
					reinterpret_cast<uintptr_t>(backup.originalTriggers),
					backup.filteredKeepAlive ?
						reinterpret_cast<uintptr_t>(backup.filteredKeepAlive->GetTriggerArray()) : 0);
				s_installLog++;
			}
		}

		void* filtered = backup.filteredKeepAlive ? backup.filteredKeepAlive->GetTriggerArray() : nullptr;
		*triggersPtr = filtered;
		*origTriggersPtr = filtered;
	}

	// Every frame: ensure the filtered triggers stay installed (engine may
	// restore originals between frames).
	static void EnsureReplacementTriggersInstalled(RE::hkbClipGenerator* a_clipGen, const std::string& /*a_replacementSuffix*/,
		float a_endClipDuration = -1.0f, bool a_keepAnnotations = false)
	{
		if (!a_clipGen) return;
		auto* bytes = reinterpret_cast<uint8_t*>(a_clipGen);
		auto* triggersPtr = reinterpret_cast<void**>(bytes + kClipGenTriggersOffset);
		auto* origTriggersPtr = reinterpret_cast<void**>(bytes + kClipGenOriginalTriggersOffset);

		std::unique_lock lock(s_triggersBackupMutex);
		auto& backup = s_triggersBackup[a_clipGen];
		void* filtered = nullptr;
		if (backup.nulled) {
			filtered = backup.filteredKeepAlive ? backup.filteredKeepAlive->GetTriggerArray() : nullptr;
			if (*triggersPtr == filtered && *origTriggersPtr == filtered) return;  // already ours
		} else {
			if (!*triggersPtr && !*origTriggersPtr) return;
			backup.triggers = *triggersPtr;
			backup.originalTriggers = *origTriggersPtr;
			backup.nulled = true;
			const auto origAnnotTexts = CollectAnimAnnotationTexts(OriginalAnimForTriggerFilter(a_clipGen));
			backup.filteredKeepAlive = BuildBehaviorOnlyTriggers(
				backup.triggers ? backup.triggers : backup.originalTriggers, origAnnotTexts,
				a_endClipDuration, a_keepAnnotations);
			filtered = backup.filteredKeepAlive ? backup.filteredKeepAlive->GetTriggerArray() : nullptr;
		}
		*triggersPtr = filtered;
		*origTriggersPtr = filtered;
	}

	static void RestoreClipTriggers(RE::hkbClipGenerator* a_clipGen)
	{
		if (!a_clipGen) return;
		auto* bytes = reinterpret_cast<uint8_t*>(a_clipGen);
		auto* triggersPtr = reinterpret_cast<void**>(bytes + kClipGenTriggersOffset);
		auto* origTriggersPtr = reinterpret_cast<void**>(bytes + kClipGenOriginalTriggersOffset);

		std::unique_lock lock(s_triggersBackupMutex);
		auto it = s_triggersBackup.find(a_clipGen);
		if (it != s_triggersBackup.end() && it->second.nulled) {
			*triggersPtr = it->second.triggers;
			*origTriggersPtr = it->second.originalTriggers;
			static int s_restoreLog = 0;
			if (s_restoreLog < 30) {
				logger::info("[OAR-Triggers] Restored clipGen={:X} triggers={:X} originalTriggers={:X}",
					reinterpret_cast<uintptr_t>(a_clipGen),
					reinterpret_cast<uintptr_t>(it->second.triggers),
					reinterpret_cast<uintptr_t>(it->second.originalTriggers));
				s_restoreLog++;
			}
			// Keep the filtered array alive briefly — the engine may still hold
			// the pointer within the current update cycle.
			RetireFilteredTriggers(std::move(it->second.filteredKeepAlive));
			s_triggersBackup.erase(it);
		}
	}

	// Thread-local flag: set to true while OAR is firing replacement annotations.
	// The event observer uses this to distinguish OAR-sourced events from engine events.
	static thread_local bool s_oarFiringAnnotations = false;

	// Direct audio playback — plays sounds through BSAudioManager by EditorID name.
	struct OARSoundHandle
	{
		uint32_t soundID{ 0 };
		bool assumeSuccess{ false };
		int8_t state{ 0 };
	};
	static_assert(sizeof(OARSoundHandle) == 0x8);

	// Returns true when a sound descriptor was found and playback started, so
	// callers with a fallback sound (dry-fire click) know whether to try it.
	static bool PlaySoundDirect(const char* a_soundName, RE::TESObjectREFR* a_refr)
	{
		if (!a_soundName || !a_soundName[0]) return false;

		// Multi-runtime IDs: OG id first, NG id second (the AE databases carry
		// the same NG function ids, so the NG slot pads forward to AE).
		// Verified against version-1-10-984 and version-1-11-221 bins.
		static REL::Relocation<void**> s_audioMgrPtr{ REL::ID({ 1321158, 2703058 }) };
		void* audioMgr = *s_audioMgrPtr;
		if (!audioMgr) return false;

		using GetSoundByName_t = void(*)(void* mgr, OARSoundHandle* handle, const char* name,
			float distance, uint32_t usageFlags, void* extraData);
		static REL::Relocation<GetSoundByName_t> GetSoundByName{ REL::ID({ 196484, 2267104 }) };

		using FadeInPlay_t = bool(*)(OARSoundHandle* handle, uint16_t ms);
		static REL::Relocation<FadeInPlay_t> FadeInPlay{ REL::ID({ 353528, 2267075 }) };

		OARSoundHandle handle{};
		GetSoundByName(audioMgr, &handle, a_soundName, 0.f, 0x1A, nullptr);

		if (handle.soundID == 0 && !handle.assumeSuccess) {
			static int s_failLog = 0;
			if (s_failLog < 30) {
				logger::info("[OAR-Audio] GetSoundHandleByName('{}') failed — no sound descriptor found", a_soundName);
				s_failLog++;
			}
			return false;
		}

		FadeInPlay(&handle, 0);

		// High cap on purpose: this line is the ground truth when diagnosing
		// missing end-of-clip sounds. The old cap of 50 ran out mid-session and
		// made annotation firing look broken when only the logging had stopped.
		static int s_playLog = 0;
		if (s_playLog < 1000) {
			logger::info("[OAR-Audio] Played sound '{}' (id={:X}) on {:X}",
				a_soundName, handle.soundID,
				a_refr ? a_refr->GetFormID() : 0u);
			s_playLog++;
		}
		return true;
	}

	// Dual-path emission: fire directly to BSTEventSource sinks (audio system, etc.)
	// so gameplay reactions to replacement annotations (reloadComplete refilling
	// the magazine, etc.) still happen even though the original triggers are
	// NULLed. Per-runtime strategy:
	//  - OG: enumerate the graph's event sources (897074) and Notify each; this
	//    reaches every registered sink (the refr, audio system, ...).
	//  - NG/AE: 897074 has no Address Library entry. Deliver straight to the
	//    actor's own BSTEventSink<BSAnimationGraphEvent> base instead
	//    (TESObjectREFR + 0x38, ProcessEvent = vfunc 1 — same layout facts as
	//    AnimGraphEventFeedHook). The refr's handler is the consumer that
	//    drives gameplay reactions; sounds already play via BSAudioManager.
	//    Verified symptom before this path existed: on AE, replaced reloads
	//    played sounds but never fired reloadComplete, so reloads never
	//    completed.
	static void NotifyEventSinks(RE::TESObjectREFR* a_refr, const RE::BSFixedString& a_evt)
	{
		if (!a_refr) return;

		// The engine delivers annotation events SPLIT at the first '.': the tag
		// is the handler selector, the payload the argument. Verified against
		// the game exe's string table, which stores the bare selector
		// "CullBone" (next to "ReloadComplete") — the bone cull handler matches
		// the TAG against that and reads the bone name from the PAYLOAD.
		// Sending the full dotted annotation text as the tag (payload empty)
		// meant the engine's CullBone/UncullBone handler ignored every
		// manually-fired cull, so magazine meshes stayed in whatever state the
		// last NATIVE trigger left them (2026-07-31: P890/SCAR reload mags).
		// Splitting applies to all dotted events, matching native dispatch.
		RE::BSFixedString tag = a_evt;
		RE::BSFixedString payload{ "" };
		if (const char* full = a_evt.c_str()) {
			if (const char* dot = std::strchr(full, '.'); dot && dot != full && dot[1] != '\0') {
				tag = RE::BSFixedString(std::string(full, dot - full).c_str());
				payload = RE::BSFixedString(dot + 1);
			}
		}

		if (REX::FModule::IsRuntimeOG()) {
			RE::BSScrapArray<RE::BSTEventSource<RE::BSAnimationGraphEvent>*> sources;
			if (!RE::BGSAnimationSystemUtils::GetEventSourcePointersFromGraph(a_refr, sources)) return;
			for (auto* src : sources) {
				if (!src) continue;
				// Fork's BSAnimationGraphEvent: holderID carries the holder ref
				// bits (bit-identical to the old refr pointer member), tag is
				// the event name, payload the argument. Members are const, so
				// build via aggregate init.
				RE::BSAnimationGraphEvent ge{ reinterpret_cast<std::uint64_t>(a_refr), tag, payload };
				src->Notify(ge);
			}
			return;
		}

		// NG/AE: virtual dispatch through the refr's own sink base. This goes
		// through the object's live vtable, so it also passes through our
		// AnimGraphEventFeedHook (which logs the event and calls the engine
		// original) — matching OG, where the registered log sink sees these too.
		RE::BSAnimationGraphEvent ge{ reinterpret_cast<std::uint64_t>(a_refr), tag, payload };
		auto* sink = reinterpret_cast<RE::BSTEventSink<RE::BSAnimationGraphEvent>*>(
			reinterpret_cast<uintptr_t>(a_refr) + 0x38);
		sink->ProcessEvent(ge, nullptr);
	}

	// Animation event observer — feeds events to the Animation Log for the UI.
	// No suppression is needed because the clone's annotationTracks are zeroed and
	// the replacement triggers we install contain the correct events.
	class OARAnnotationSuppressionSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent& a_event,
			RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
		{
			const char* evtStr = a_event.tag.c_str();
			if (!evtStr || !evtStr[0]) return RE::BSEventNotifyControl::kContinue;

			// holderID carries the same bits the old refr pointer member did.
			auto* refr = reinterpret_cast<RE::TESObjectREFR*>(a_event.holderID);

			// Feed every animation event to the Animation Log for the UI.
			// Events with a payload (CullBone.X, SoundPlay.X, ...) display as
			// tag.payload — the engine splits dotted annotations on dispatch,
			// so tag alone would read as just "CullBone".
			if (AnimationLog::GetSingleton()->IsEnabled() && refr) {
				std::string display(evtStr);
				if (const char* pay = a_event.payload.c_str(); pay && pay[0]) {
					display += '.';
					display += pay;
				}
				AnimationLog::GetSingleton()->AddAnimEvent(refr, display);
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		bool registered{ false };
	};

	static OARAnnotationSuppressionSink s_suppressionSink;

	// Match the reference plugin's stale-state safeguard. Opening either menu
	// interrupts special-idle playback; a pending flag must not leak forward and
	// consume an unrelated IdleStop later in the session.
	class IdleStopMenuWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (a_event.opening &&
				(a_event.menuName == "LoadingMenu" || a_event.menuName == "PipboyMenu")) {
				ClearIdleStopSuppressionState();
				logger::info("[OAR-IdleStop] Cleared pending suppression on '{}' open", a_event.menuName.c_str());
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		bool registered{ false };
	};

	static IdleStopMenuWatcher s_idleStopMenuWatcher;

	void RegisterSuppressionSink()
	{
		if (!s_idleStopMenuWatcher.registered) {
			if (auto* ui = RE::UI::GetSingleton()) {
				if (auto* source = ui->GetEventSource<RE::MenuOpenCloseEvent>()) {
					source->RegisterSink(&s_idleStopMenuWatcher);
					s_idleStopMenuWatcher.registered = true;
					logger::info("[OAR-IdleStop] Registered menu reset watcher");
				}
			}
		}

		if (s_suppressionSink.registered) return;

		// OG-only: GetEventSourcePointersFromGraph (897074) has no NG/AE ID.
		// On NG/AE the equivalent feed comes from AnimGraphEventFeedHook (a
		// BSTEventSink<BSAnimationGraphEvent> ProcessEvent vfunc hook on the
		// Actor/PlayerCharacter vtables), installed at hook time.
		if (!REX::FModule::IsRuntimeOG()) {
			static bool s_logged = false;
			if (!s_logged) {
				s_logged = true;
				logger::info("[OAR-Annot] Registered-sink path skipped on this runtime; event log fed by the BSTEventSink vfunc hook instead");
			}
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) return;

		RE::BSScrapArray<RE::BSTEventSource<RE::BSAnimationGraphEvent>*> sources;
		if (RE::BGSAnimationSystemUtils::GetEventSourcePointersFromGraph(player, sources)) {
			for (auto* src : sources) {
				if (src) src->RegisterSink(&s_suppressionSink);
			}
			s_suppressionSink.registered = true;
			logger::info("[OAR-Annot] Registered annotation suppression sink ({} sources)", sources.size());
		}
	}

}

// Weapon change detection — called from Activate when the weapon animation
// folder or equipped weapon instance changes. Runtime clones are retired only
// after the activating binding has been scrubbed and every surviving recorded
// binding slot has had its exact clone restored to its validated game original.
static std::string s_lastKnownWeaponFolder;
static std::shared_mutex s_lastKnownWeaponMutex;
static uint64_t s_lastKnownEquippedFingerprint{ 0 };
static bool s_lastKnownEquippedFingerprintValid{ false };

// Restore clones from the exact binding slots recorded when OAR installed
// them. This runs BEFORE clone retirement, while AnimationCache can still
// validate the live clone's original by pointer, duration, and track count.
//
// The exact *slot == clone check is essential. A weapon graph transition can
// free or recycle a binding before the next Activate; readable memory alone is
// not proof that the old slot still belongs to us. If the slot changed, leave
// it untouched. StartEcho's retired-clone restore remains the fallback for any
// stale shared binding that was not recorded or could not be restored here.
static size_t RestoreRecordedBindingSlotsBeforeWeaponInvalidation()
{
	std::vector<std::pair<RE::hkaAnimation**, BindingSlotBackup>> backups;
	{
		std::shared_lock lock(s_bindingSlotBackupMutex);
		backups.reserve(s_bindingSlotBackup.size());
		for (const auto& entry : s_bindingSlotBackup) backups.push_back(entry);
	}

	auto* cache = AnimationCache::GetSingleton();
	size_t restored = 0;
	for (const auto& [slot, backup] : backups) {
		if (!slot || !backup.clone || !backup.original) continue;
		if (IsBadReadPtr(slot, sizeof(void*)) || *slot != backup.clone) continue;

		// Use the cache reverse link instead of trusting the backup by itself.
		// GetOriginalFromReplacement also verifies that the recorded duration and
		// transform-track count still match the pointed-to game animation.
		auto* original = cache->GetOriginalFromReplacement(backup.clone);
		if (!original || original != backup.original ||
			IsBadReadPtr(original, sizeof(uintptr_t))) {
			continue;
		}
		if (!IsPlausibleGameAnimVtable(*reinterpret_cast<uintptr_t*>(original))) continue;

		*slot = original;
		restored++;
	}

	if (restored > 0) {
		logger::info("[OAR-WeaponChange] Restored {} recorded binding slot(s) before clone retirement", restored);
	}
	return restored;
}

static uint64_t GetPlayerWeaponFingerprint()
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player || !player->currentProcess || !player->currentProcess->middleHigh) {
		return 0;
	}

	struct WeaponIdentity
	{
		uint32_t formID;
		uintptr_t instanceData;
		uint32_t equipIndex;
	};
	std::vector<WeaponIdentity> equippedWeapons;
	auto* middleHigh = player->currentProcess->middleHigh;
	{
		RE::BSAutoLock lock{ middleHigh->equippedItemsLock };
		for (auto& equipped : middleHigh->equippedItems) {
			auto* object = equipped.item.object;
			if (object && object->GetFormType() == RE::ENUM_FORM_ID::kWEAP) {
				equippedWeapons.push_back({
					object->GetFormID(),
					reinterpret_cast<uintptr_t>(equipped.item.instanceData.get()),
					equipped.equipIndex.index
				});
			}
		}
	}

	std::sort(equippedWeapons.begin(), equippedWeapons.end(), [](const auto& a_lhs, const auto& a_rhs) {
		if (a_lhs.formID != a_rhs.formID) return a_lhs.formID < a_rhs.formID;
		if (a_lhs.instanceData != a_rhs.instanceData) return a_lhs.instanceData < a_rhs.instanceData;
		return a_lhs.equipIndex < a_rhs.equipIndex;
	});
	uint64_t fingerprint = 1469598103934665603ull;
	auto mix = [&](uint64_t a_value) {
		fingerprint ^= a_value;
		fingerprint *= 1099511628211ull;
	};
	for (const auto& weapon : equippedWeapons) {
		mix(weapon.formID);
		mix(weapon.instanceData);
		mix(weapon.equipIndex);
	}
	return fingerprint;
}

static void CheckAndInvalidateOnWeaponChange()
{
	std::string currentFolder;
	{
		std::shared_lock lock(s_graphAnimPathMutex);
		currentFolder = s_weaponAnimFolder;
	}
	const auto currentEquippedFingerprint = GetPlayerWeaponFingerprint();

	std::unique_lock lock(s_lastKnownWeaponMutex);
	const bool folderChanged = !currentFolder.empty() && currentFolder != s_lastKnownWeaponFolder;
	const bool equippedSetChanged = s_lastKnownEquippedFingerprintValid &&
		currentEquippedFingerprint != s_lastKnownEquippedFingerprint;
	if (!s_lastKnownEquippedFingerprintValid) {
		s_lastKnownEquippedFingerprint = currentEquippedFingerprint;
		s_lastKnownEquippedFingerprintValid = true;
	}

	if (folderChanged || equippedSetChanged) {
		logger::info("[OAR-WeaponChange] Weapon state changed: folder '{}' -> '{}', weapon fingerprint {:X} -> {:X} — retiring runtime clones",
			s_lastKnownWeaponFolder, currentFolder,
			s_lastKnownEquippedFingerprint, currentEquippedFingerprint);
		s_lastKnownWeaponFolder = currentFolder;
		s_lastKnownEquippedFingerprint = currentEquippedFingerprint;
		lock.unlock();

		RestoreRecordedBindingSlotsBeforeWeaponInvalidation();
		AnimationCache::GetSingleton()->InvalidateRuntimeClones();
		// Clone retirement alone is insufficient for weapons that share an animation
		// folder. The engine can recycle the same clip generator and binding while
		// s_activeSubModMap, suffix/path caches, and variant state still describe the
		// previously equipped weapon. Clear those locks on every equipped-set change,
		// not only when the graph folder changes. This deliberately does not call
		// RestoreAllActiveReplacements: weapon transitions can already have freed
		// recorded originals, so writing those pointers back would be unsafe.
		ClearClipRuntimeState();
	} else if (currentFolder.empty() && !s_lastKnownWeaponFolder.empty()) {
		// Weapon unequipped (holstered or no weapon) — also invalidate
		logger::info("[OAR-WeaponChange] Weapon folder cleared (was '{}') — retiring runtime clones",
			s_lastKnownWeaponFolder);
		s_lastKnownWeaponFolder.clear();
		lock.unlock();

		RestoreRecordedBindingSlotsBeforeWeaponInvalidation();
		AnimationCache::GetSingleton()->InvalidateRuntimeClones();
		ClearClipRuntimeState();
	}
}

void RegisterWeaponEquipListener()
{
	// No-op: weapon change detection is handled inline via CheckAndInvalidateOnWeaponChange()
	// which is called from the Activate hook. CommonLibF4 for FO4 doesn't expose TESEquipEvent.
	logger::info("[OAR-Equip] Using inline weapon-change detection in Activate hook");
}

// Restore every hooked clip's original animation and triggers while the
// recorded original pointers are STILL VALID. Must run at kPreLoadGame BEFORE
// ClearClipRuntimeState()/InvalidateRuntimeClones() wipe the bookkeeping.
//
// WHY: a clip that carries our replacement across a save load becomes an
// unrecoverable orphan — the wiped maps mean we can never un-replace it, so
// condition changes mid-clip stop working for it ([OAR-RecoveryFail]).
// Restoring here means no clip carries a replacement across the load at all.
// Not safe for the weapon-switch invalidation path (old originals may already
// be freed there) — this is save-load only, where the engine hasn't torn
// anything down yet at the time the message arrives.
// Returns true only if a_clip still looks like a LIVE hkbClipGenerator:
// readable through the fields we touch AND carrying the correct vtable.
// The vtable check is the load-bearing part — a save load can free clip
// generators WITHOUT firing our Deactivate hook (wholesale graph teardown),
// and IsBadReadPtr alone passes for freed-but-still-mapped allocator pages.
// Crash-2026-07-21-05-33-55: clip+0xD0 held FLT_MAX bit-pattern garbage from
// a recycled allocation; the vtable of recycled memory won't match ours.
static bool IsLiveClipGenerator(const RE::hkbClipGenerator* a_clip)
{
	// Cover the full range of fields we read/write (vtable .. originalTriggers @0xD8).
	if (!a_clip || IsBadReadPtr(a_clip, 0xE0)) return false;
	const auto vtbl = *reinterpret_cast<const uintptr_t*>(a_clip);
	return vtbl == Offsets::hkbClipGenerator_vtbl.address();
}

// GetAnimationSlot() with every interior pointer validated before dereference.
// Mirrors hkbClipGenerator::GetAnimationSlot (animCtrl@+0xD0 -> binding@+0x38
// -> animation slot@+0x18) but never trusts a hop blindly.
static RE::hkaAnimation** SafeGetAnimationSlot(const RE::hkbClipGenerator* a_clip)
{
	auto* bytes = reinterpret_cast<const uint8_t*>(a_clip);
	auto* ctrl = *reinterpret_cast<uint8_t* const*>(bytes + 0xD0);
	if (!ctrl || IsBadReadPtr(ctrl, 0x40)) return nullptr;
	auto* bind = *reinterpret_cast<uint8_t* const*>(ctrl + 0x38);
	if (!bind || IsBadReadPtr(bind, 0x20)) return nullptr;
	auto** slot = reinterpret_cast<RE::hkaAnimation**>(const_cast<uint8_t*>(bind) + 0x18);
	if (IsBadReadPtr(slot, sizeof(void*))) return nullptr;
	return slot;
}

void RestoreAllActiveReplacements()
{
	auto* cache = AnimationCache::GetSingleton();
	size_t restoredAnims = 0;
	size_t skippedDead = 0;

	{
		std::unique_lock lock(s_originalAnimMutex);
		for (auto& [clip, original] : s_originalAnimMap) {
			if (!clip || !original) continue;
			// A save load can tear down graphs (freeing clip generators) without
			// our Deactivate hook firing, so map entries may be dangling here.
			// Only touch clips that still carry the hkbClipGenerator vtable.
			if (!IsLiveClipGenerator(clip)) { skippedDead++; continue; }
			auto** slot = SafeGetAnimationSlot(clip);
			if (!slot) continue;
			// Only touch slots that currently hold OUR replacement.
			if (!cache->IsOurReplacement(*slot)) continue;
			// Validate the original before writing it back. Accept ANY plausible
			// game-module hkaAnimation vtable (spline / interleaved / ...), not
			// only the single captured type — exact matching skipped valid
			// originals during restore (same fix as GetValidOriginal).
			if (IsBadReadPtr(original, sizeof(uintptr_t))) continue;
			auto vtbl = *reinterpret_cast<uintptr_t*>(original);
			if (!IsPlausibleGameAnimVtable(vtbl)) continue;
			// Single aligned pointer write — same operation as a normal swap.
			// Benign vs the render thread: both old and new pointers stay
			// valid (the clone buffer is retired, not freed, right after).
			*slot = original;
			restoredAnims++;
		}
	}

	// Second pass, keyed by BINDING SLOT (see BindingSlotBackup). This is the
	// pass that actually works on a mid-session save load: the clip-keyed pass
	// above skips everything because the clips are already dead, but the
	// bindings those slots live in are cached by the engine and survive the
	// load. Restoring the game original here is what prevents a retired clone
	// from leaking into the NEW session's clips as an unrecoverable orphan
	// (wrong/original annotations, [OAR-RecoveryFail] spam — 2026-08-01 log).
	size_t restoredSlots = 0;
	{
		std::unique_lock bsLock(s_bindingSlotBackupMutex);
		for (auto& [slot, backup] : s_bindingSlotBackup) {
			if (!slot || !backup.clone || !backup.original) continue;
			if (IsBadReadPtr(slot, sizeof(void*))) continue;
			// Only touch slots that STILL hold the exact clone we recorded.
			// Anything else means the binding was freed/recycled or the slot
			// was already restored — leave it alone.
			if (*slot != backup.clone) continue;
			if (IsBadReadPtr(backup.original, sizeof(uintptr_t))) continue;
			auto vtbl = *reinterpret_cast<uintptr_t*>(backup.original);
			if (!IsPlausibleGameAnimVtable(vtbl)) continue;
			*slot = backup.original;
			restoredSlots++;
		}
		s_bindingSlotBackup.clear();
	}
	if (restoredSlots > 0) {
		logger::info("[OAR-PreLoad] Restored {} surviving binding slots to game originals", restoredSlots);
	}

	// Restore all NULL'd triggers so native annotations work after the load.
	size_t restoredTriggers = 0;
	{
		std::unique_lock lock(s_triggersBackupMutex);
		for (auto& [clip, backup] : s_triggersBackup) {
			if (!clip || !backup.nulled) continue;
			// Same liveness rule as above — writing triggers into freed/recycled
			// memory would silently corrupt whatever now lives there.
			if (!IsLiveClipGenerator(clip)) { skippedDead++; continue; }
			auto* bytes = reinterpret_cast<uint8_t*>(clip);
			*reinterpret_cast<void**>(bytes + kClipGenTriggersOffset) = backup.triggers;
			*reinterpret_cast<void**>(bytes + kClipGenOriginalTriggersOffset) = backup.originalTriggers;
			restoredTriggers++;
		}
		// ClearClipRuntimeState (called right after) clears the map itself.
	}

	if (skippedDead > 0) {
		logger::info("[OAR-PreLoad] Skipped {} dead/freed clip entries during restore", skippedDead);
	}

	if (restoredAnims > 0 || restoredTriggers > 0) {
		logger::info("[OAR-PreLoad] Restored {} animation slots and {} trigger sets before state wipe",
			restoredAnims, restoredTriggers);
	}
}

void ClearClipRuntimeState()
{
	ClearIdleStopSuppressionState();
	{
		std::unique_lock lock(s_originalAnimMutex);
		s_originalAnimMap.clear();
	}
	{
		std::unique_lock lock(s_vanillaAnnotMutex);
		s_vanillaAnnotMap.clear();
		s_vanillaAnnotCount.store(0, std::memory_order_relaxed);
	}
	{
		std::lock_guard lock(s_annotIntegrityMutex);
		s_annotIntegrityLastT.clear();
	}
	{
		// Binding identities are keyed by GAME animation pointers, which a
		// save load frees — never let a recycled address inherit a path.
		std::unique_lock lock(s_bindingSuffixMutex);
		s_bindingRealPath.clear();
	}
	{
		// Slot backups are consumed by RestoreAllActiveReplacements on a save
		// load; clear here too for the weapon-switch invalidation path, where
		// the old bindings (and thus these slot addresses) are being freed.
		std::unique_lock lock(s_bindingSlotBackupMutex);
		s_bindingSlotBackup.clear();
	}
	{
		std::unique_lock lock(s_clipSuffixMutex);
		s_clipSuffixCache.clear();
	}
	{
		std::unique_lock lock(s_clipRealPathMutex);
		s_clipRealPathCache.clear();
	}
	{
		std::unique_lock lock(s_clipRealPathStateMutex);
		s_clipRealPathAuthoritative.clear();
		s_clipRealPathAttempts.clear();
		s_pendingActivateLog.clear();
	}
	{
		std::unique_lock lock(s_playerClipMutex);
		s_playerClipGraph.clear();
	}
	{
		std::unique_lock lock(s_clipVariantMutex);
		s_clipVariantCache.clear();
	}
	{
		std::unique_lock lock(s_annotStateMutex);
		s_annotStateMap.clear();
	}
	{
		std::unique_lock lock(s_activeReplacementSuffixMutex);
		s_activeReplacementSuffixes.clear();
	}
	{
		std::unique_lock lock(s_origAnnotSetMutex);
		s_origAnnotByActor.clear();
	}
	{
		std::unique_lock lock(s_activeReplacementActorMutex);
		s_activeReplacementByActor.clear();
	}
	{
		std::unique_lock lock(s_triggersBackupMutex);
		// Retire installed filtered trigger arrays instead of freeing them:
		// surviving clips may still have their trigger slots pointing at these
		// buffers (a NULL slot was always safe; a freed buffer is not).
		for (auto& [clip, backup] : s_triggersBackup) {
			RetireFilteredTriggers(std::move(backup.filteredKeepAlive));
		}
		s_triggersBackup.clear();
	}
	{
		std::unique_lock lock(s_bypassMutex);
		s_bypassSet.clear();
	}
	{
		std::unique_lock lock(s_builtTriggersMutex);
		s_builtTriggers.clear();
	}
	// NOTE: Do NOT clear s_loadClipsPathMap here - it persists across save loads.
	// LoadClips Hook #1 only fires during initial game startup (project loading),
	// not on subsequent save loads. The stringData pointers remain valid.
	{
		std::shared_lock lock(s_loadClipsPathMutex);
		logger::info("[OAR] Preserving LoadClips path map ({} entries) across save load", s_loadClipsPathMap.size());
	}
	{
		std::unique_lock lock(s_idleAnimReverseMutex);
		s_idleAnimReverseMap.clear();
	}
	s_idleAnimReverseBuilt.store(false);
	// Reset weapon anim folder (will be re-populated on next clip activation)
	{
		std::unique_lock lock(s_graphAnimPathMutex);
		s_graphAnimPathByIndex.clear();
		s_weaponAnimFolder.clear();
		s_weaponAnimFolderValid.store(false);
	}
	// Don't clear s_createFile* maps - they persist across save loads
	// (CreateFileW captures are valid globally)
	// Don't clear s_subGraphIDToFolder - accumulated mapping persists
	s_lastWeaponSubGraphID.store(0);
	ActiveReplacementTracker::GetSingleton()->Clear();
	logger::info("[OAR] Cleared clip runtime state (preserved LoadClips/CreateFile path maps)");
}

// ===== Global enable/disable (Settings > General > "Enabled") =================
// The Settings checkbox is ticked on the UI (render) thread, but restoring
// animation slots and trigger arrays must happen on the game thread — the same
// rule every other swap we perform follows. So the toggle only raises a flag;
// HookedActorUpdate drains it on the next frame and performs the real restore.
static std::atomic<bool> s_pendingGlobalDisableRestore{ false };
// Deferred to the game thread for the same reason as the disable restore:
// the reload frees every SubMod / ReplacementAnimFileInfo that live graph
// updates may be holding pointers to.
static std::atomic<bool> s_pendingConfigReload{ false };
static std::atomic<bool> s_pendingLookupResort{ false };

// Runs ON THE GAME THREAD (from HookedActorUpdate). Puts every hooked clip
// back on its vanilla animation and clears all replacement-active state so
// nothing keeps applying while disabled. Path caches and lookup tables are
// deliberately preserved: re-enabling resumes replacement on the next clip
// update without waiting for paths to re-resolve.
static void PerformGlobalDisableRestore()
{
	ClearIdleStopSuppressionState();
	// 1) Write game originals back into every clip slot that still holds our
	//    clone, and re-install original trigger arrays (validated — same code
	//    path as the save-load restore).
	RestoreAllActiveReplacements();

	// 2) Drop the trigger backups: the originals are physically back in place.
	//    Installed filtered arrays are retired (kept alive), not freed — the
	//    engine may still hold a pointer within the current update cycle.
	{
		std::unique_lock lock(s_triggersBackupMutex);
		for (auto& [clip, backup] : s_triggersBackup) {
			RetireFilteredTriggers(std::move(backup.filteredKeepAlive));
		}
		s_triggersBackup.clear();
	}
	{
		std::lock_guard lock(s_triggersRestoredMutex);
		s_triggersRestoredSet.clear();
	}

	// 3) Stop track filters and full-body blends immediately — the Generate
	//    and actor-update paths early-out once these counts hit zero.
	{
		std::unique_lock lock(s_trackFilterMutex);
		s_charTrackFilterMap.clear();
		s_trackFilterActiveCount.store(0, std::memory_order_relaxed);
	}
	{
		std::unique_lock lock(s_fullBodyBlendMutex);
		s_fullBodyBlendMap.clear();
		s_fullBodyBlendActiveCount.store(0, std::memory_order_relaxed);
	}

	// 4) Clear per-clip replacement bookkeeping (active submods, annotation
	//    cursors, and the "Active Replacements" UI/API views).
	{
		std::unique_lock lock(s_activeSubModMutex);
		s_activeSubModMap.clear();
		s_activeSubModBinding.clear();
	}
	{
		std::unique_lock lock(s_annotStateMutex);
		s_annotStateMap.clear();
	}
	{
		std::unique_lock lock(s_activeReplacementSuffixMutex);
		s_activeReplacementSuffixes.clear();
	}
	{
		std::unique_lock lock(s_activeReplacementActorMutex);
		s_activeReplacementByActor.clear();
	}
	ActiveReplacementTracker::GetSingleton()->Clear();

	logger::info("[OAR] Global disable: restored all replaced clips to vanilla animations");
}

void OnGlobalEnabledChanged(bool a_enabled)
{
	if (a_enabled) {
		logger::info("[OAR] Global enable toggled ON — replacement resumes on the next clip update");
		return;
	}
	logger::info("[OAR] Global enable toggled OFF — queueing vanilla restore on the game thread");
	s_pendingGlobalDisableRestore.store(true);
}

void RequestConfigReload()
{
	logger::info("[OAR] Config reload requested — queued to run on the game thread");
	s_pendingConfigReload.store(true);
}

void RequestLookupResort()
{
	s_pendingLookupResort.store(true);
}

namespace
{
	RE::TESObjectREFR* GetRefrFromContext(const RE::hkbContext* a_context)
	{
		if (!a_context) return nullptr;
		auto* character = a_context->character;
		if (!character) return nullptr;

		// Fast path: check existing cache (without the mainBodyCharacters filter)
		{
			std::shared_lock lock(s_characterCacheMutex);
			auto it = s_characterCache.find(character);
			if (it != s_characterCache.end()) {
				auto* refr = it->second;
				if (refr && refr->As<RE::Actor>()) return refr;
			}
		}

		// Cache miss: try to register the player (most common case for missing characters)
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (player) {
			RegisterActorCharacter(player);
			std::shared_lock lock(s_characterCacheMutex);
			auto it = s_characterCache.find(character);
			if (it != s_characterCache.end()) return it->second;
		}

		return nullptr;
	}

	// Classify a clip as 1st- or 3rd-person from its animation file path.
	//
	// Rule (user-confirmed, matches GunMover's displayed paths): first-person
	// character animations live under "...\_1stPerson\..." and first-person
	// WEAPON animations under "...\1stPerson<Weapon>\..." directory forms —
	// so any "1stperson" occurrence marks 1st person. Everything else is
	// treated as 3rd person (body, power armor, MT/idle animations, etc.).
	//
	// NOTE: classification via a_context->character->projectData is NOT viable
	// here — verified in-game that the context's character is a static dummy
	// with no project data, and even real graph characters have an empty
	// hkbProjectStringData::animationPath in this runtime.
	AnimationLog::Perspective ClassifyPerspectiveFromPath(const std::string& a_path)
	{
		using Perspective = AnimationLog::Perspective;

		if (a_path.empty()) return Perspective::kUnknown;

		std::string lower = a_path;
		std::ranges::transform(lower, lower.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::ranges::replace(lower, '/', '\\');

		return lower.find("1stperson") != std::string::npos ?
			Perspective::kFirstPerson : Perspective::kThirdPerson;
	}

	// LoadedIdleAnimData: mirrors engine struct at REL::ID(762973)
	struct LoadedIdleAnimDataRaw
	{
		RE::BSFixedString animFile;
		uint64_t          unk2;
		void*             binding;
		void*             clipGenerator;
		void*             animationGraph;
	};
	static_assert(sizeof(LoadedIdleAnimDataRaw) == 40);

	struct BSTArrayHeaderRaw
	{
		void*    data;
		uint32_t size;
		uint32_t capacity;
	};
	static_assert(sizeof(BSTArrayHeaderRaw) == 16);

	// SEH-guarded bounded C-string copy. Returns the copied length, or -1 if
	// the source memory faulted mid-read. Its own function because __try
	// cannot share a frame with objects that need C++ unwinding.
	static int SafeCopyCString(const char* a_src, char* a_dst, size_t a_cap) noexcept
	{
		__try {
			size_t i = 0;
			for (; i + 1 < a_cap; i++) {
				const char c = a_src[i];
				a_dst[i] = c;
				if (c == '\0') return static_cast<int>(i);
			}
			a_dst[a_cap - 1] = '\0';
			return static_cast<int>(a_cap - 1);
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return -1;
		}
	}

	// Only path-shaped strings are useful to the reverse map: its one consumer
	// extracts an animation suffix from a real .hkx path. The engine array
	// holds stale entries whose animFile points at freed or reused memory --
	// the 2026-08-23 16:52 session captured binary junk and a stray hkb
	// expression string ("Decel = cond(...)") in this map -- so anything that
	// does not read as printable ASCII ending in .hkx is dropped, not trusted.
	static bool LooksLikeAnimPath(const char* a_str, int a_len)
	{
		if (a_len < 5 || a_len >= 260) return false;
		for (int i = 0; i < a_len; i++) {
			const auto c = static_cast<unsigned char>(a_str[i]);
			if (c < 0x20 || c > 0x7E) return false;
		}
		return _stricmp(a_str + a_len - 4, ".hkx") == 0;
	}

	static void BuildIdleAnimReverseMap()
	{
		if (s_idleAnimReverseBuilt.load()) return;

		// OG-only: the LoadedIdleAnimData array global (762973) has no NG/AE
		// Address Library entry. On NG/AE the idle reverse map stays empty;
		// idle path resolution falls back to the other capture sources
		// (CreateFileW redirects, string-data scan, clip bindings).
		if (!REX::FModule::IsRuntimeOG()) {
			static std::atomic_bool s_warned{ false };
			if (!s_warned.exchange(true)) {
				logger::warn("[OAR-IdleAnim] Idle reverse map unavailable on this runtime (OG-only engine global); idle paths resolve via fallback sources");
			}
			return;
		}

		REL::Relocation<BSTArrayHeaderRaw*> arrReloc{ REL::ID(762973) };
		auto* arrHeader = arrReloc.get();
		if (!arrHeader) {
			logger::warn("[OAR-IdleAnim] Array relocation returned null");
			return;
		}
		if (IsBadReadPtr(arrHeader, sizeof(BSTArrayHeaderRaw))) {
			logger::warn("[OAR-IdleAnim] Array header not readable");
			return;
		}
		if (!arrHeader->data || arrHeader->size == 0 || arrHeader->size > 100000) {
			logger::warn("[OAR-IdleAnim] Array not ready (size={}, data={:X})",
				arrHeader->size, reinterpret_cast<uintptr_t>(arrHeader->data));
			return;
		}
		if (IsBadReadPtr(arrHeader->data, sizeof(LoadedIdleAnimDataRaw))) {
			logger::warn("[OAR-IdleAnim] Array data pointer invalid");
			return;
		}

		// One builder at a time; losers skip rather than queue. The map is a
		// best-effort resolution source with three fallbacks, so "not this
		// frame" is fine -- and skipping means no thread ever waits here.
		static std::atomic<bool> s_building{ false };
		bool buildExpected = false;
		if (!s_building.compare_exchange_strong(buildExpected, true)) return;

		// Every engine-memory read below runs with NO lock held, behind an
		// SEH-guarded bounded copy. The 2026-08-23 16:52 hang: a stale
		// animFile string AV'd while this loop held the unique_lock; the
		// outer SEH wrapper (SafeCallOriginalUpdate) swallowed the fault
		// WITHOUT running the lock's destructor -- that is how __except
		// unwinds under /EHsc -- so s_idleAnimReverseMutex stayed locked
		// forever and the next BuildIdleAnimReverseMap call deadlocked the
		// main thread against the abandoned lock. The lock now guards only
		// the swap-in of a fully built list; nothing inside it can fault.
		auto* entries = reinterpret_cast<LoadedIdleAnimDataRaw*>(arrHeader->data);
		std::vector<std::pair<RE::hkbClipGenerator*, std::string>> collected;
		collected.reserve(arrHeader->size);
		int captured = 0;
		int skipped = 0;
		char fileNameBuffer[512];
		for (uint32_t i = 0; i < arrHeader->size; i++) {
			auto& e = entries[i];
			// The array carries stale rows (a clipGenerator of 0xFFFFFFFF was
			// captured in the field), so the clip pointer gets the same
			// plausibility test as the string pointer.
			const auto rawClipPtr = reinterpret_cast<uintptr_t>(e.clipGenerator);
			if (rawClipPtr < 0x10000 || rawClipPtr > 0x7FFFFFFFFFFFull) continue;

			// BSFixedString stores a pointer at offset 0; validate before calling c_str()
			auto rawStrPtr = *reinterpret_cast<const uintptr_t*>(&e.animFile);
			if (rawStrPtr == 0 || rawStrPtr < 0x10000 || rawStrPtr > 0x7FFFFFFFFFFFull) {
				skipped++;
				continue;
			}
			if (IsBadReadPtr(reinterpret_cast<void*>(rawStrPtr), 8)) {
				skipped++;
				continue;
			}

			const char* fileName = e.animFile.c_str();
			if (!fileName || reinterpret_cast<uintptr_t>(fileName) < 0x10000) continue;
			const int len = SafeCopyCString(fileName, fileNameBuffer, sizeof(fileNameBuffer));
			if (len <= 0 || !LooksLikeAnimPath(fileNameBuffer, len)) {
				skipped++;
				continue;
			}
			collected.emplace_back(
				reinterpret_cast<RE::hkbClipGenerator*>(e.clipGenerator),
				std::string(fileNameBuffer, static_cast<size_t>(len)));
			captured++;
		}
		{
			std::unique_lock lock(s_idleAnimReverseMutex);
			for (auto& [clipGen, name] : collected) {
				s_idleAnimReverseMap[clipGen] = std::move(name);
			}
		}

		s_idleAnimReverseBuilt.store(true);
		s_building.store(false);
		logger::info("[OAR-IdleAnim] Built reverse map: {} entries ({} skipped) from {} total",
			captured, skipped, arrHeader->size);

		int logged = 0;
		std::shared_lock lock(s_idleAnimReverseMutex);
		for (auto& [clipPtr, name] : s_idleAnimReverseMap) {
			if (logged >= 10) break;
			logger::info("[OAR-IdleAnim]   clip={:X} -> '{}'",
				reinterpret_cast<uintptr_t>(clipPtr), name);
			logged++;
		}
	}

	static std::shared_mutex s_nameLookupMutex;
	static std::unordered_map<std::string, std::string> s_suffixToReplacementPath;
	// Sorted replacement info per suffix (highest priority first)
	static std::unordered_map<std::string, std::vector<ReplacementAnimFileInfo*>> s_suffixToInfos;
	// Leaf-name -> all full suffixes that share the same leaf (for multi-match evaluation).
	// E.g., "wpnreload" -> ["scar\wpnreload", "scar\60rddrum\wpnreload", "wpnreload"]
	static std::unordered_map<std::string, std::vector<std::string>> s_leafToFullSuffixes;
	// Leaf-name -> suffixes registered by submods with Leaf Matching enabled
	// ("match by filename, outrank path matches"). Presence of a leaf here
	// forces the multi-match path for EVERY clip sharing that filename — even
	// clips whose exact path IS registered or direct-path resolved — so the
	// flagged submods get first shot. Rebuilt with the priority resort (the
	// flag is UI-editable). Guarded by s_nameLookupMutex.
	static std::unordered_map<std::string, std::vector<std::string>> s_leafOverrideSuffixes;
	static std::vector<std::unique_ptr<std::string>> s_persistentStrings;
	static bool s_lookupBuilt = false;

	// s_originalAnimMap declared above ClearClipRuntimeState()

	// Extract the leaf name from a suffix for comparison purposes.
	// "scar\wpnidleready" → "wpnidleready", "multi:wpnidleready" → "wpnidleready"
	static std::string_view GetSuffixLeaf(const std::string& a_suffix)
	{
		std::string_view sv(a_suffix);
		if (sv.size() > 6 && sv.substr(0, 6) == "multi:") {
			sv = sv.substr(6);
		}
		auto lastSlash = sv.rfind('\\');
		if (lastSlash != std::string_view::npos) {
			sv = sv.substr(lastSlash + 1);
		}
		return sv;
	}

	static bool LeafEndsWithAdd(std::string_view a_leaf)
	{
		if (a_leaf.size() < 3) return false;
		const auto tail = a_leaf.substr(a_leaf.size() - 3);
		return std::tolower(static_cast<unsigned char>(tail[0])) == 'a' &&
			std::tolower(static_cast<unsigned char>(tail[1])) == 'd' &&
			std::tolower(static_cast<unsigned char>(tail[2])) == 'd';
	}

	static std::string ExtractAnimSuffix(const std::string& a_path)
	{
		auto lower = a_path;
		std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::ranges::replace(lower, '/', '\\');

		auto pos = lower.find("animations\\");
		if (pos != std::string::npos) {
			auto suffix = lower.substr(pos + 11);
			auto dot = suffix.rfind('.');
			if (dot != std::string::npos) suffix = suffix.substr(0, dot);
			return suffix;
		}

		auto dot = lower.rfind('.');
		if (dot != std::string::npos) lower = lower.substr(0, dot);
		auto lastSep = lower.rfind('\\');
		if (lastSep != std::string::npos && lastSep > 0) {
			auto prevSep = lower.rfind('\\', lastSep - 1);
			if (prevSep != std::string::npos)
				return lower.substr(prevSep + 1);
		}
		return lower;
	}

	// Leaf Matching override: when any submod with the flag registers this
	// suffix's FILENAME, the clip must go through multi-match evaluation so
	// the flagged submod gets first shot — even when a_suffix is an exact,
	// directly-registered (or direct-path resolved) suffix that would
	// otherwise short-circuit straight to path matching. Returns
	// "multi:<leaf>" in that case, a_suffix unchanged otherwise.
	// Caller must hold NO locks on s_nameLookupMutex.
	static std::string ApplyLeafOverride(const std::string& a_suffix)
	{
		if (a_suffix.empty() || a_suffix.rfind("multi:", 0) == 0) return a_suffix;
		std::string leaf = a_suffix;
		if (auto lastSlash = a_suffix.rfind('\\'); lastSlash != std::string::npos) {
			leaf = a_suffix.substr(lastSlash + 1);
		}
		std::shared_lock rlock(s_nameLookupMutex);
		if (s_leafOverrideSuffixes.find(leaf) != s_leafOverrideSuffixes.end()) {
			return std::string("multi:") + leaf;
		}
		return a_suffix;
	}

	// Given a raw suffix, check if it's directly registered. If so, return it.
	// If not, extract the leaf name and check the multi-leaf lookup table.
	// Returns "multi:<leaf>" if multiple candidates exist, the single candidate
	// if only one exists, or the original suffix if no leaf match is found.
	// A leaf-override registration (see ApplyLeafOverride) forces multi mode
	// before either check.
	// Caller must hold NO locks on s_nameLookupMutex.
	static std::string ResolveOrLeafFallback(const std::string& a_suffix)
	{
		if (a_suffix.empty()) return a_suffix;

		if (auto overridden = ApplyLeafOverride(a_suffix); overridden != a_suffix) {
			return overridden;
		}

		{
			std::shared_lock rlock(s_nameLookupMutex);
			if (s_suffixToInfos.find(a_suffix) != s_suffixToInfos.end()) {
				return a_suffix;
			}
		}

		std::string leaf = a_suffix;
		auto lastSlash = a_suffix.rfind('\\');
		if (lastSlash != std::string::npos) {
			leaf = a_suffix.substr(lastSlash + 1);
		}

		if (leaf == a_suffix) return a_suffix;

		std::shared_lock rlock(s_nameLookupMutex);
		auto leafIt = s_leafToFullSuffixes.find(leaf);
		if (leafIt == s_leafToFullSuffixes.end() || leafIt->second.empty()) {
			return a_suffix;
		}

		if (leafIt->second.size() == 1) {
			return leafIt->second[0];
		}

		return std::string("multi:") + leaf;
	}

	// Build s_leafToFullSuffixes and s_leafOverrideSuffixes from the current
	// s_suffixToInfos. Caller must hold s_nameLookupMutex (unique).
	// Ordering rule for each leaf's suffix list: leaf-override suffixes first
	// (so multi-match probes give flagged submods first shot), then longest
	// (most specific) path first — the pre-existing rule that keeps bare-leaf
	// fallback registrations from hijacking folder-scoped candidates.
	static void RebuildLeafTablesLocked()
	{
		auto leafOf = [](const std::string& a_suffix) {
			auto lastSlash = a_suffix.rfind('\\');
			return lastSlash != std::string::npos ? a_suffix.substr(lastSlash + 1) : a_suffix;
		};
		auto hasLeafFlag = [](const std::vector<ReplacementAnimFileInfo*>& a_infos) {
			for (auto* info : a_infos) {
				if (info && info->parentSubMod && info->parentSubMod->GetLeafMatching()) return true;
			}
			return false;
		};

		// Leaf-to-full-suffix map for multi-match evaluation.
		// E.g., for suffixes "scar\wpnreload" and "wpnreload", both map to leaf "wpnreload".
		s_leafToFullSuffixes.clear();
		s_leafOverrideSuffixes.clear();
		for (auto& [suffix, infos] : s_suffixToInfos) {
			auto leaf = leafOf(suffix);
			s_leafToFullSuffixes[leaf].push_back(suffix);
			if (hasLeafFlag(infos)) {
				s_leafOverrideSuffixes[leaf].push_back(suffix);
			}
		}

		auto isOverride = [&](const std::string& a_suffix) {
			auto it = s_leafOverrideSuffixes.find(leafOf(a_suffix));
			if (it == s_leafOverrideSuffixes.end()) return false;
			return std::ranges::find(it->second, a_suffix) != it->second.end();
		};
		auto maxPriority = [&](const std::string& a_suffix) {
			int best = INT_MIN;
			auto it = s_suffixToInfos.find(a_suffix);
			if (it != s_suffixToInfos.end()) {
				for (auto* info : it->second) {
					if (info && info->parentSubMod) best = std::max(best, info->parentSubMod->GetPriority());
				}
			}
			return best;
		};

		for (auto& [leaf, suffixes] : s_leafToFullSuffixes) {
			std::ranges::sort(suffixes, [&](const auto& a, const auto& b) {
				const bool oa = isOverride(a), ob = isOverride(b);
				if (oa != ob) return oa;
				if (a.size() != b.size()) return a.size() > b.size();
				return a < b;
			});
			if (suffixes.size() > 1) {
				logger::info("[OAR] LeafLookup: leaf='{}' -> {} candidates: [{}]",
					leaf, suffixes.size(),
					[&]() {
						std::string joined;
						for (size_t i = 0; i < suffixes.size(); i++) {
							if (i > 0) joined += ", ";
							joined += "'" + suffixes[i] + "'";
						}
						return joined;
					}());
			}
		}

		// Override list order = foreign-claim preference: highest priority
		// first, then the LEAST specific (shortest) suffix — a submod that
		// mirrors several per-grip paths should serve its base file to foreign
		// weapons, and a fully deterministic order keeps the claimed suffix
		// (and its track-filter state identity) stable across frames. Clips on
		// the submod's own weapon never reach this order: the pre-pass tries
		// their exact registered suffix first.
		for (auto& [leaf, suffixes] : s_leafOverrideSuffixes) {
			std::ranges::sort(suffixes, [&](const auto& a, const auto& b) {
				const int pa = maxPriority(a), pb = maxPriority(b);
				if (pa != pb) return pa > pb;
				if (a.size() != b.size()) return a.size() < b.size();
				return a < b;
			});
			logger::info("[OAR] LeafOverride: leaf='{}' has {} leaf-matching suffix(es); filename matching outranks path matching for this leaf",
				leaf, suffixes.size());
		}
	}

	static void BuildNameLookup()
	{
		std::unique_lock lock(s_nameLookupMutex);
		if (s_lookupBuilt) return;

		auto* oar = OpenAnimationReplacer::GetSingleton();
		const auto& pathMap = oar->GetPathToSubModsMap();

		for (auto& [mapKey, replacementInfos] : pathMap) {
			auto suffix = ExtractAnimSuffix(mapKey);
			if (suffix.empty()) continue;

			auto& infoVec = s_suffixToInfos[suffix];
			for (auto& info : replacementInfos) {
				s_suffixToReplacementPath[suffix] = info.replacementPath;
				infoVec.push_back(const_cast<ReplacementAnimFileInfo*>(&info));
			}

			// Sort by priority (highest first)
			std::ranges::sort(infoVec, [](const auto* a, const auto* b) {
				int pa = a->parentSubMod ? a->parentSubMod->GetPriority() : 0;
				int pb = b->parentSubMod ? b->parentSubMod->GetPriority() : 0;
				return pa > pb;
			});

			if (!infoVec.empty() && Settings::GetSingleton()->bVerboseLogging) {
				logger::info("[OAR] NameLookup: suffix='{}' -> '{}' ({} candidates)",
					suffix, infoVec[0]->replacementPath, infoVec.size());
			}
		}

		RebuildLeafTablesLocked();

		s_lookupBuilt = true;
		logger::info("[OAR] Built name lookup with {} suffix entries, {} leaf entries",
			s_suffixToReplacementPath.size(), s_leafToFullSuffixes.size());
	}

	// Re-sorts every per-suffix candidate vector by CURRENT SubMod priority.
	// The vectors are sorted once at build time and the winner-selection loops
	// (EvaluateWinningInfo and the Update hook) take the first passing entry,
	// so a priority edited in the UI has no effect until the order is redone.
	// GAME THREAD ONLY (drained from HookedActorUpdate): sorting permutes the
	// vectors in place while clip hooks may hold pointers to them, so it must
	// be serialized with graph updates the same way the config reload is.
	static void ResortNameLookupByPriority()
	{
		std::unique_lock lock(s_nameLookupMutex);
		if (!s_lookupBuilt) return;
		for (auto& [suffix, infoVec] : s_suffixToInfos) {
			std::ranges::sort(infoVec, [](const auto* a, const auto* b) {
				int pa = a->parentSubMod ? a->parentSubMod->GetPriority() : 0;
				int pb = b->parentSubMod ? b->parentSubMod->GetPriority() : 0;
				return pa > pb;
			});
		}
		// The Leaf Matching flag is UI-editable and its effect lives in the leaf
		// tables (override membership + probe ordering), so rebuild them with
		// every resort — same trigger, same game-thread serialization.
		RebuildLeafTablesLocked();
		logger::info("[OAR] Name lookup re-sorted by priority ({} suffix entries)",
			s_suffixToInfos.size());
	}

	static void PreloadReplacementAnimations()
	{
		auto* oar = OpenAnimationReplacer::GetSingleton();
		auto* cache = AnimationCache::GetSingleton();
		const auto& pathMap = oar->GetPathToSubModsMap();

		// Flatten the work list up front. The path map is stable for the whole
		// preload (parsing finished before this runs; LoadAnimation never adds
		// map entries), so raw info pointers are safe to hand to workers.
		//
		// Load EVERY SubMod's file for each original path. The cache keys
		// entries per (suffix, owning SubMod), so the Update hook can play
		// the condition-winning SubMod's actual file — previously only one
		// file per suffix was cached and a lower-priority mod's file could
		// play under a higher-priority mod's name (or vice versa).
		struct PreloadItem
		{
			std::string suffix;
			const ReplacementAnimFileInfo* info;
		};
		std::vector<PreloadItem> work;
		std::atomic<int> failed{ 0 };

		for (auto& [mapKey, replacementInfos] : pathMap) {
			auto suffix = ExtractAnimSuffix(mapKey);
			if (suffix.empty()) continue;

			for (auto& info : replacementInfos) {
				if (info.absoluteDiskPath.empty()) {
					logger::warn("[OAR-Preload] No absolute path for suffix '{}'", suffix);
					failed.fetch_add(1, std::memory_order_relaxed);
					continue;
				}
				work.push_back({ suffix, &info });
			}
		}

		// Progress bar denominator must match what LoadAnimation increments
		// (loadingLoadedAnims), not the ReplacementAnimation object count.
		// Variant groups are ONE ReplacementAnimation but N cache files
		// (base + __v1/__v2/...), and multiple SubMods can each contribute a
		// file for the same original path — that used to show as "40/36".
		oar->loadingTotalAnims.store(static_cast<int>(work.size()));
		oar->loadingLoadedAnims.store(0);
		oar->loadingParsedAnims.store(static_cast<int>(work.size()));

		std::atomic<int> loaded{ 0 };
		std::atomic<size_t> nextItem{ 0 };

		auto runWorker = [&]() {
			for (;;) {
				const size_t i = nextItem.fetch_add(1, std::memory_order_relaxed);
				if (i >= work.size()) break;
				const auto& item = work[i];
				if (cache->LoadAnimation(item.suffix, item.info->absoluteDiskPath, item.info->parentSubMod,
						item.info->parentSubMod ? item.info->parentSubMod->GetPriority() : 0)) {
					loaded.fetch_add(1, std::memory_order_relaxed);
				} else {
					failed.fetch_add(1, std::memory_order_relaxed);
				}
			}
		};

		// Parallel load (same bAsyncParsing toggle upstream OAR uses for its
		// std::async parsing). Safe because LoadAnimation only touches
		// entry-local data outside the cache mutex, and every shared sink —
		// cache map, progress counters, log sink — is thread-safe. File opens
		// dominate the cost on Windows (the AV scans each loose file on open),
		// so overlapping them scales nearly linearly with thread count.
		unsigned threadCount = 1;
		if (Settings::GetSingleton()->bAsyncParsing && work.size() > 1) {
			threadCount = std::clamp(std::thread::hardware_concurrency(), 2u, 16u);
			threadCount = std::min(threadCount, static_cast<unsigned>(work.size()));
		}

		const auto start = std::chrono::high_resolution_clock::now();
		if (threadCount <= 1) {
			runWorker();
		} else {
			std::vector<std::thread> workers;
			workers.reserve(threadCount);
			for (unsigned t = 0; t < threadCount; t++) {
				workers.emplace_back(runWorker);
			}
			for (auto& w : workers) {
				w.join();
			}
		}
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now() - start)
		                    .count();

		logger::info("[OAR-Preload] Pre-loaded {} animations ({} failed) on {} thread(s) in {}ms, cache size: {}",
			loaded.load(), failed.load(), threadCount, ms, cache->GetCacheSize());
	}

	// Full "Reload All Configs" implementation. GAME THREAD ONLY — drained
	// from HookedActorUpdate, outside the Havok update cycle.
	//
	// History: this used to run directly on the UI (render) thread via the
	// job queue, freeing every SubMod / ReplacementAnimFileInfo while the
	// game thread was mid graph-update holding pointers into them
	// (crash-2026-07-31-04-28-26). Worse, nothing ever invalidated the name
	// lookup: s_suffixToInfos kept pointers into the FREED old
	// animPathToReplacementsMap forever after a reload, and only heap-block
	// reuse made it look like it worked.
	static void PerformConfigReload()
	{
		// The startup load may still be running on its background thread (the
		// reload job can be queued from the UI at the main menu). Re-parsing
		// concurrently with it would race ClearAllMods against the parser —
		// join it first; no-op once the startup load has finished.
		OpenAnimationReplacer::GetSingleton()->WaitForBackgroundLoad();

		logger::info("[OAR] Config reload (game thread): restoring vanilla state before re-parse");

		// 1) Physically restore originals/triggers into every replaced clip and
		//    drop all runtime maps holding SubMod*/info pointers (same restore
		//    the global Enabled toggle uses).
		PerformGlobalDisableRestore();

		// Play-once decisions lock a replace/no-replace choice for a clip's
		// lifetime; a decision made against the OLD config must not gag the
		// new one (long-lived loops like sprints would keep it for minutes).
		{
			std::unique_lock lock(s_playOnceDecisionMutex);
			s_playOnceDecision.clear();
		}

		// 2) Invalidate the name lookup BEFORE freeing what it points into.
		{
			std::unique_lock lock(s_nameLookupMutex);
			s_suffixToInfos.clear();
			s_suffixToReplacementPath.clear();
			s_leafToFullSuffixes.clear();
			s_lookupBuilt = false;
		}

		// 3) Tear down and re-parse all mod configurations.
		auto* oar = OpenAnimationReplacer::GetSingleton();
		oar->ClearAllMods();
		Parsing::ParseAllMods();

		// 4) Rebuild everything derived from the parsed data, mirroring the
		//    startup sequence (ParseAllMods -> TryDeferredInjection).
		//
		//    The animation cache is NOT cleared: entries are re-bound to the
		//    new SubMod owners by file identity (path + size + mtime) inside
		//    LoadAnimation, so unchanged files cost no disk I/O — only files
		//    actually edited since load are re-read. Entries whose file left
		//    the config (submod deleted/renamed) are pruned afterwards.
		auto* cache = AnimationCache::GetSingleton();
		cache->MarkAllForRebind();
		BuildNameLookup();
		Hooks::FileRedirectHooks::BuildFileRedirectMap();
		SetHasActiveReplacements(oar->GetTotalReplacementCount() > 0);
		PreloadReplacementAnimations();
		if (const auto pruned = cache->PruneUnrebound(); pruned > 0) {
			logger::info("[OAR] Config reload: pruned {} cached animation file(s) whose submod no longer exists",
				pruned);
		}

		logger::info("[OAR] Config reload complete: {} replacement animations",
			oar->GetTotalReplacementCount());
	}

	// ========================================================================
	// Selected-subgraph animation path resolution (deterministic)
	//
	// Resolves the REAL on-disk animation path for a clip by reading the
	// engine's "selected subgraph" data on the owning BShkbAnimationGraph.
	// This is the same data the engine itself uses to decide which weapon
	// animation directory to load clips from, so when it succeeds it is
	// authoritative — no leaf-name guessing required.
	//
	// Algorithm (verified via RE; same technique as GunMover's subgraph path):
	//   1. From the active clip context, get the active/current behavior graph.
	//   2. Read behaviorGraph+0x30. In these subgraph cases this identifies the
	//      selected subgraph/root and can also point back to the owning
	//      BShkbAnimationGraph; validate against RE::VTABLE::BShkbAnimationGraph
	//      if using it as a graph pointer.
	//   3. On the owning BShkbAnimationGraph, read the selected subgraph swap
	//      array at graph+0x3A0.
	//   4. Iterate entries of size 0x48.
	//   5. For each entry, read sharedData at entry+0x08.
	//   6. Match the active behavior graph root id against sharedData+0xC0.
	//   7. The selected data block is sharedData-0x40.
	//   8. Read selected animation directory/file arrays from:
	//        - data+0x178, count at +0x188
	//        - fallback: data+0x160, count at +0x170
	//      These arrays contain BSFixedString entries.
	//   9. Take the leaf basename from clip->animationName, force .hkx, append
	//      it to each selected directory, and probe with BSResourceNiBinaryStream.
	//      The first existing resource is the real displayed path.
	// ========================================================================

	// Raw offsets for the selected-subgraph walk (see algorithm above)
	constexpr uintptr_t kCtx_BehaviorGraph = 0x08;        // hkbContext -> active hkbBehaviorGraph
	constexpr uintptr_t kBG_RootId = 0x30;                // hkbBehaviorGraph -> selected subgraph/root id
	constexpr uintptr_t kGraph_SwapArrayPtr = 0x3A0;      // BShkbAnimationGraph -> swap array (BSTArray*)
	constexpr uintptr_t kSwap_EntrySize = 0x48;           // stride of each swap array entry
	constexpr uintptr_t kSwap_SharedData = 0x08;          // entry -> sharedData
	constexpr uintptr_t kShared_RootId = 0xC0;            // sharedData -> owning root id (match vs kBG_RootId)
	constexpr uintptr_t kShared_ToDataBlock = 0x40;       // data block = sharedData - 0x40
	constexpr uintptr_t kData_FileArrayPrimary = 0x178;   // BSFixedString array (BSTArray: data +0, size +0x10)
	constexpr uintptr_t kData_FileArrayFallback = 0x160;  // BSFixedString array (BSTArray: data +0, size +0x10)
	constexpr uintptr_t kGraph_EmbeddedCharacter = 0x1C8; // BShkbAnimationGraph -> embedded hkbCharacter (the
	                                                      // hkbContext::character for clips of this graph points here)

	// Cache of resource-existence probes (normalized lowercase path -> exists)
	// so each unique candidate is only probed once per session.
	static std::shared_mutex s_subgraphProbeMutex;
	static std::unordered_map<std::string, bool> s_subgraphProbeCache;

	// Normalize separators to '\' and strip trailing slashes.
	static std::string SubgraphNormalizePath(std::string a_path)
	{
		for (auto& ch : a_path) {
			if (ch == '/') ch = '\\';
		}
		while (!a_path.empty() && a_path.back() == '\\') {
			a_path.pop_back();
		}
		return a_path;
	}

	static std::string SubgraphToLower(std::string a_value)
	{
		std::ranges::transform(a_value, a_value.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a_value;
	}

	// RAII probe for engine resource existence (archives + loose files).
	// Our CommonLibF4 fork's BSResourceNiBinaryStream header hardcodes NG-only
	// REL IDs (2269830/2269832), but this plugin targets pre-NG 1.10.163, so we
	// construct the 0x30-byte stream object in a raw buffer using the pre-NG
	// ctor/dtor IDs (cross-referenced from GunMover's dual-ID CommonLib fork:
	// Ctor { 1198116, 2269830 }, Dtor { 1516202, 2269832 }).
	struct SubgraphResourceProbe
	{
		alignas(8) uint8_t buffer[0x30]{};

		explicit SubgraphResourceProbe(const char* a_path)
		{
			// void BSResourceNiBinaryStream::ctor(this, fileName, writeable, location, fullReadHint)
			// Multi-runtime: { OG, NG } (NG id also present in the AE bins).
			using Ctor_t = void (*)(void*, const char*, bool, void*, bool);
			static REL::Relocation<Ctor_t> ctor{ REL::ID({ 1198116, 2269830 }) };
			ctor(buffer, a_path, false, nullptr, false);
		}

		~SubgraphResourceProbe()
		{
			using Dtor_t = void (*)(void*);
			static REL::Relocation<Dtor_t> dtor{ REL::ID({ 1516202, 2269832 }) };
			dtor(buffer);
		}

		// The stream smart pointer at +0x10 is non-null when the resource exists
		// (mirrors BSResourceNiBinaryStream::operator bool).
		[[nodiscard]] bool exists() const
		{
			return *reinterpret_cast<void* const*>(buffer + 0x10) != nullptr;
		}
	};

	// Returns true if the given path exists as an engine resource. Probes both
	// the raw path and with a "Meshes\" prefix (animation paths are usually
	// relative to Meshes). Results are cached.
	static bool SubgraphResourceExists(const std::string& a_path)
	{
		if (a_path.empty()) return false;

		const auto normalized = SubgraphNormalizePath(a_path);
		const auto key = SubgraphToLower(normalized);
		{
			std::shared_lock lock(s_subgraphProbeMutex);
			auto it = s_subgraphProbeCache.find(key);
			if (it != s_subgraphProbeCache.end()) return it->second;
		}

		bool exists = false;
		{
			SubgraphResourceProbe probe(normalized.c_str());
			exists = probe.exists();
		}
		if (!exists && !key.starts_with("meshes\\")) {
			const auto meshesPath = std::string("Meshes\\") + normalized;
			SubgraphResourceProbe probe(meshesPath.c_str());
			exists = probe.exists();
		}

		{
			std::unique_lock lock(s_subgraphProbeMutex);
			s_subgraphProbeCache[key] = exists;
		}
		return exists;
	}

	// Extract lowercase leaf basename (no directories, no extension) from a path.
	static std::string SubgraphGetLeaf(const char* a_path)
	{
		if (!a_path || a_path[0] == '\0') return {};
		auto path = SubgraphNormalizePath(a_path);
		const auto slash = path.rfind('\\');
		if (slash != std::string::npos) path = path.substr(slash + 1);
		const auto dot = path.rfind('.');
		if (dot != std::string::npos) path = path.substr(0, dot);
		return SubgraphToLower(path);
	}

	// Build a candidate full path from a selected-subgraph array entry and the
	// clip's leaf name. Entries can be either directories ("Weapons\SCAR") or
	// full file paths ("...\WPNReload.hkx"); handle both (step 9).
	static std::string SubgraphBuildCandidate(const char* a_entryPath, const std::string& a_leaf)
	{
		if (a_leaf.empty() || !a_entryPath || a_entryPath[0] == '\0') return {};

		auto entry = SubgraphNormalizePath(a_entryPath);
		if (entry.empty()) return {};

		const auto lowerEntry = SubgraphToLower(entry);
		if (lowerEntry.ends_with(".hkx") || lowerEntry.ends_with(".hkt")) {
			// Full file entry: only usable if its leaf matches the clip's leaf.
			// Force the extension to .hkx (the on-disk form).
			if (SubgraphGetLeaf(entry.c_str()) == a_leaf) {
				const auto dot = entry.rfind('.');
				return entry.substr(0, dot) + ".hkx";
			}
			return {};
		}

		// Directory entry: append leaf + ".hkx"
		return entry + "\\" + a_leaf + ".hkx";
	}

	// Decode a BSStringPool::Entry* — the storage behind BSFixedString — to its
	// character data. The selected-subgraph file arrays hold BSFixedStrings,
	// whose single pointer member is NOT a char*: it points at a 0x18-byte pool
	// entry header (left ptr +0x00, flags +0x08, crc +0x0A, length/right union
	// +0x10) with the string bytes following the header. "Shallow" entries
	// (flag 1<<14) chain via the +0x10 pointer to the leaf entry that owns the
	// bytes. This mirrors BSFixedString::c_str() -> Entry::leaf()/u8().
	// Reading the header bytes as text (the previous behavior) yields garbage,
	// which is why the scan below never probed a real path. Returns nullptr on
	// any invalid/wide entry.
	static const char* SubgraphDecodePoolEntry(uintptr_t a_entry)
	{
		constexpr uint16_t kShallow = 1 << 14;
		constexpr uint16_t kWide = 1 << 15;

		for (int hops = 0; hops < 8; ++hops) {
			if (!a_entry || a_entry < 0x10000 ||
				IsBadReadPtr(reinterpret_cast<void*>(a_entry), 0x18)) {
				return nullptr;
			}
			const auto flags = *reinterpret_cast<const uint16_t*>(a_entry + 0x08);
			if (flags & kShallow) {
				// Shallow entry: follow _right (+0x10) to the leaf
				a_entry = *reinterpret_cast<const uintptr_t*>(a_entry + 0x10);
				continue;
			}
			if (flags & kWide) {
				// wchar_t entry — never expected for animation paths
				return nullptr;
			}
			// Leaf entry: character data starts right after the 0x18-byte header
			const auto* str = reinterpret_cast<const char*>(a_entry + 0x18);
			return IsBadReadPtr(str, 1) ? nullptr : str;
		}
		return nullptr;
	}

	// Defined below (steps 8-9): scans the selected file arrays for the leaf.
	static std::string SubgraphScanFileArrays(uintptr_t a_data, const std::string& a_leaf);

	// Search ONE BShkbAnimationGraph's swap array (graph+0x3A0) for the entry
	// whose sharedData root id (+0xC0) matches a_rootId. Returns the selected
	// data block (sharedData-0x40) or 0. The graph pointer is vtable-validated.
	static uintptr_t SubgraphFindSwapData(uintptr_t a_graph, uintptr_t a_rootId)
	{
		static REL::Relocation<uintptr_t> bshkbVtbl{ RE::VTABLE::BShkbAnimationGraph[0] };

		if (!a_graph || !a_rootId || a_graph < 0x10000 ||
			IsBadReadPtr(reinterpret_cast<void*>(a_graph), kGraph_SwapArrayPtr + 8) ||
			*reinterpret_cast<uintptr_t*>(a_graph) != bshkbVtbl.address()) {
			return 0;
		}

		// Swap array pointer at graph+0x3A0 (BSTArray: data +0, size +0x10)
		const auto swapArray = *reinterpret_cast<uintptr_t*>(a_graph + kGraph_SwapArrayPtr);
		if (!swapArray || IsBadReadPtr(reinterpret_cast<void*>(swapArray), 0x18)) {
			return 0;
		}

		const auto entries = *reinterpret_cast<uintptr_t*>(swapArray);
		const auto count = *reinterpret_cast<uint32_t*>(swapArray + 0x10);
		if (!entries || count == 0 || count > 0x100 ||
			IsBadReadPtr(reinterpret_cast<void*>(entries), count * kSwap_EntrySize)) {
			return 0;
		}

		for (uint32_t i = 0; i < count; ++i) {
			const auto entry = entries + static_cast<uintptr_t>(i) * kSwap_EntrySize;
			const auto sharedData = *reinterpret_cast<uintptr_t*>(entry + kSwap_SharedData);
			if (!sharedData || IsBadReadPtr(reinterpret_cast<void*>(sharedData), kShared_RootId + 8)) {
				continue;
			}
			if (*reinterpret_cast<uintptr_t*>(sharedData + kShared_RootId) == a_rootId) {
				return sharedData - kShared_ToDataBlock;
			}
		}
		return 0;
	}

	// Resolve one (owningGraph, nestedGraph) pair exactly like GunMover's
	// ResolveFromSelectedSubgraphFiles: match the nested graph's root id
	// (+0x30) against the owning graph's swap array, then scan+probe the
	// selected file arrays. Empty result on any failure.
	static std::string SubgraphResolvePair(uintptr_t a_owningGraph, uintptr_t a_nestedGraph, const std::string& a_leaf)
	{
		if (!a_nestedGraph || a_nestedGraph < 0x10000 ||
			IsBadReadPtr(reinterpret_cast<void*>(a_nestedGraph), kBG_RootId + 8)) {
			return {};
		}
		const auto rootId = *reinterpret_cast<uintptr_t*>(a_nestedGraph + kBG_RootId);
		if (!rootId) return {};

		const auto dataBlock = SubgraphFindSwapData(a_owningGraph, rootId);
		if (!dataBlock) return {};
		return SubgraphScanFileArrays(dataBlock, a_leaf);
	}

	// GunMover's ResolveOwningGraphFromBehaviorGraph: the nested graph's root
	// id (+0x30) can itself point back at the owning BShkbAnimationGraph —
	// vtable-validate it and use it as the owner, else use the fallback graph.
	static uintptr_t SubgraphResolveOwningGraph(uintptr_t a_nestedGraph, uintptr_t a_fallbackGraph)
	{
		static REL::Relocation<uintptr_t> bshkbVtbl{ RE::VTABLE::BShkbAnimationGraph[0] };

		if (!a_nestedGraph || a_nestedGraph < 0x10000 ||
			IsBadReadPtr(reinterpret_cast<void*>(a_nestedGraph), kBG_RootId + 8)) {
			return a_fallbackGraph;
		}
		const auto rootId = *reinterpret_cast<uintptr_t*>(a_nestedGraph + kBG_RootId);
		if (!rootId || rootId < 0x10000 ||
			IsBadReadPtr(reinterpret_cast<void*>(rootId), sizeof(void*)) ||
			*reinterpret_cast<uintptr_t*>(rootId) != bshkbVtbl.address()) {
			return a_fallbackGraph;
		}
		return rootId;
	}

	// GunMover's GetAllClipInfo walk, restricted to finding ONE clip: walk the
	// root graph's hkbBehaviorGraph (+0x378) activeNodes array (+0xE0), find
	// the hkbNodeInfo entry whose node IS a_clip (identity match), and resolve
	// via that entry's nested behavior graph (+0x10). The owning graph is the
	// nested graph's root id when it points back at a BShkbAnimationGraph,
	// otherwise the walked root graph — exactly GunMover's owner resolution.
	static std::string SubgraphResolveViaRootGraphWalk(uintptr_t a_rootGraph, RE::hkbClipGenerator* a_clip, const std::string& a_leaf)
	{
		constexpr uintptr_t kBShkb_HkRootGraph = 0x378;  // BShkbAnimationGraph -> root hkbBehaviorGraph
		constexpr uintptr_t kBG_ActiveNodes = 0xE0;      // hkbBehaviorGraph -> hkArray<hkbNodeInfo*>*

		static REL::Relocation<uintptr_t> bshkbVtbl{ RE::VTABLE::BShkbAnimationGraph[0] };

		if (!a_rootGraph || a_rootGraph < 0x10000 ||
			IsBadReadPtr(reinterpret_cast<void*>(a_rootGraph), kBShkb_HkRootGraph + 8) ||
			*reinterpret_cast<uintptr_t*>(a_rootGraph) != bshkbVtbl.address()) {
			return {};
		}
		const auto hkGraph = *reinterpret_cast<uintptr_t*>(a_rootGraph + kBShkb_HkRootGraph);
		if (!hkGraph || hkGraph < 0x10000 ||
			IsBadReadPtr(reinterpret_cast<void*>(hkGraph), 0x1B0)) {
			return {};
		}

		// GunMover skips the walk while the graph is rebuilding its node list
		// (updateActiveNodes at +0x1AC, stateOrTransitionChanged at +0x1AD).
		if (*reinterpret_cast<const uint8_t*>(hkGraph + 0x1AC) != 0 ||
			*reinterpret_cast<const uint8_t*>(hkGraph + 0x1AD) != 0) {
			return {};
		}

		// hkArray layout: data +0, size (int32) +8
		const auto activeNodes = *reinterpret_cast<uintptr_t*>(hkGraph + kBG_ActiveNodes);
		if (!activeNodes || IsBadReadPtr(reinterpret_cast<void*>(activeNodes), 0x10)) {
			return {};
		}
		const auto data = *reinterpret_cast<uintptr_t*>(activeNodes);
		const auto size = *reinterpret_cast<int32_t*>(activeNodes + 8);
		if (!data || size <= 0 || size > 0x1000 ||
			IsBadReadPtr(reinterpret_cast<void*>(data), static_cast<size_t>(size) * sizeof(void*))) {
			return {};
		}

		const auto clipAddr = reinterpret_cast<uintptr_t>(a_clip);
		for (int32_t i = 0; i < size; ++i) {
			const auto entry = *reinterpret_cast<uintptr_t*>(data + static_cast<uintptr_t>(i) * sizeof(void*));
			if (!entry || IsBadReadPtr(reinterpret_cast<void*>(entry), 0x18)) {
				continue;
			}
			// GunMover's SelectActiveClip: the entry itself may be the clip, or
			// the clip sits at entry+0x08 (hkbNodeInfo::node). Identity match only.
			const auto node = *reinterpret_cast<uintptr_t*>(entry + 0x08);
			if (entry != clipAddr && node != clipAddr) {
				continue;
			}

			// Found our clip's entry: nested behavior graph at +0x10
			const auto nested = *reinterpret_cast<uintptr_t*>(entry + 0x10);
			const auto owner = SubgraphResolveOwningGraph(nested, a_rootGraph);
			return SubgraphResolvePair(owner, nested, a_leaf);
		}
		return {};
	}

	// Steps 8-9: scan the selected directory/file arrays and probe candidates.
	// Returns the first candidate path that exists as an engine resource.
	static std::string SubgraphScanFileArrays(uintptr_t a_data, const std::string& a_leaf)
	{
		const auto scanArray = [&](uintptr_t a_arrayOffset) -> std::string {
			const auto array = a_data + a_arrayOffset;
			if (IsBadReadPtr(reinterpret_cast<void*>(array), 0x18)) return {};

			const auto entries = *reinterpret_cast<uintptr_t*>(array);
			const auto size = *reinterpret_cast<uint32_t*>(array + 0x10);
			if (!entries || size == 0 || size > 0x400 ||
				IsBadReadPtr(reinterpret_cast<void*>(entries), size * sizeof(void*))) {
				return {};
			}

			for (uint32_t i = 0; i < size; ++i) {
				// Each element is a BSFixedString: one pointer to a string-pool
				// entry, NOT to raw characters — decode it properly.
				const auto strPtr = *reinterpret_cast<uintptr_t*>(entries + static_cast<uintptr_t>(i) * sizeof(void*));
				const auto* entryPath = SubgraphDecodePoolEntry(strPtr);
				if (!entryPath || entryPath[0] == '\0') continue;

				auto candidate = SubgraphBuildCandidate(entryPath, a_leaf);
				if (!candidate.empty() && SubgraphResourceExists(candidate)) {
					return candidate;
				}
			}
			return {};
		};

		// Primary array at data+0x178 (count at +0x188), fallback at data+0x160 (count at +0x170)
		if (auto result = scanArray(kData_FileArrayPrimary); !result.empty()) return result;
		if (auto result = scanArray(kData_FileArrayFallback); !result.empty()) return result;
		return {};
	}

	// Full resolution: clip + context -> real on-disk animation path ("" on failure).
	static std::string ResolveClipPathFromSubgraph(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context)
	{
		if (!a_this || !a_context ||
			reinterpret_cast<uintptr_t>(a_context) < 0x10000 ||
			IsBadReadPtr(a_context, kCtx_BehaviorGraph + 8)) {
			return {};
		}

		// Step 9 input: leaf basename from the clip's authored animation name.
		// At Update time animationName may already be cleared by the engine, so
		// fall back to the authored path backfilled into the display cache at
		// Activate time (non-authoritative entry).
		auto leaf = SubgraphGetLeaf(a_this->animationName.data());
		if (leaf.empty()) {
			std::shared_lock plock(s_clipRealPathMutex);
			auto pit = s_clipRealPathCache.find(a_this);
			if (pit != s_clipRealPathCache.end()) {
				leaf = SubgraphGetLeaf(pit->second.c_str());
			}
		}
		if (leaf.empty()) return {};

		// Faithful GunMover algorithm, per root graph: find THIS clip's entry in
		// the root graph's activeNodes, take the nested behavior graph from that
		// entry, resolve the owning graph from the nested graph's root id (with
		// the walked root as fallback), and match/scan/probe. GunMover only
		// walks the player's 3rd-person root (variableCache.graphToCacheFor);
		// we walk BOTH player roots because OAR hooks clips from the 1st-person
		// graph too — each walk is still a strict (owner, nested) pair, never a
		// cross-product, so a clip resolves only through its own entry. This
		// prevents wrong-weapon matches (e.g. stale "44pistol" directories from
		// another subgraph's swap entry).
		static REL::Relocation<uintptr_t> bshkbVtblLocal{ RE::VTABLE::BShkbAnimationGraph[0] };
		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
			if (player->GetAnimationGraphManagerImpl(manager) && manager) {
				for (uint32_t i = 0; i < manager->graph.size() && i < 4; ++i) {
					const auto root = reinterpret_cast<uintptr_t>(manager->graph[i].get());
					if (auto path = SubgraphResolveViaRootGraphWalk(root, a_this, leaf); !path.empty()) {
						return path;
					}
				}
			}
		}

		// Clip not found in a player root graph's activeNodes (non-player actor,
		// or activeNodes mid-rebuild). Same strict pair resolution via the
		// clip's own hkbNodeInfo (identical object to the activeNodes entry:
		// node at +0x08, nested graph at +0x10).
		//
		// Owner candidates, in order:
		//   1. The nested graph's own root id when it points back at a real
		//      BShkbAnimationGraph (GunMover's primary owner resolution).
		//   2. The context's character-embedded graph (character - 0x1C8) when
		//      it is a real BShkbAnimationGraph (GunMover's fallback owner).
		//   3. Each of the player's root graphs. Safe even when wrong: the
		//      swap-array lookup only accepts an entry whose sharedData root id
		//      (+0xC0) EXACTLY equals this clip's nested-graph root id, so a
		//      foreign owner simply yields no match — it can never return a
		//      different weapon's directory set.
		if (auto* nodeInfo = a_this->nodeInfo; nodeInfo &&
			reinterpret_cast<uintptr_t>(nodeInfo) > 0x10000 &&
			!IsBadReadPtr(nodeInfo, 0x18)) {
			const auto base = reinterpret_cast<uintptr_t>(nodeInfo);
			if (*reinterpret_cast<uintptr_t*>(base + 0x08) == reinterpret_cast<uintptr_t>(a_this)) {
				const auto nested = *reinterpret_cast<uintptr_t*>(base + 0x10);

				// Candidate 1: nested graph's root id as owner
				const auto ownerFromRootId = SubgraphResolveOwningGraph(nested, 0);
				if (ownerFromRootId) {
					if (auto path = SubgraphResolvePair(ownerFromRootId, nested, leaf); !path.empty()) {
						return path;
					}
				}

				// Candidate 2: context's character-embedded graph
				if (a_context->character &&
					!IsBadReadPtr(a_context->character, sizeof(void*))) {
					const auto candidate = reinterpret_cast<uintptr_t>(a_context->character) - kGraph_EmbeddedCharacter;
					if (!IsBadReadPtr(reinterpret_cast<void*>(candidate), sizeof(void*)) &&
						*reinterpret_cast<uintptr_t*>(candidate) == bshkbVtblLocal.address() &&
						candidate != ownerFromRootId) {
						if (auto path = SubgraphResolvePair(candidate, nested, leaf); !path.empty()) {
							return path;
						}
					}
				}

				// Candidate 3: player's root graphs (exact root-id match keeps this safe)
				if (auto* player = RE::PlayerCharacter::GetSingleton()) {
					RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
					if (player->GetAnimationGraphManagerImpl(manager) && manager) {
						for (uint32_t i = 0; i < manager->graph.size() && i < 4; ++i) {
							const auto root = reinterpret_cast<uintptr_t>(manager->graph[i].get());
							if (!root || root == ownerFromRootId) continue;
							if (auto path = SubgraphResolvePair(root, nested, leaf); !path.empty()) {
								return path;
							}
						}
					}
				}
			}
		}

		return {};
	}

	// One-shot gate for poll failure diagnostics: the same few active clips
	// fail every frame, so log each clip's failure only once.
	static bool SubgraphShouldLogPollFailure(RE::hkbClipGenerator* a_clip)
	{
		static std::mutex s_mutex;
		static std::unordered_set<RE::hkbClipGenerator*> s_logged;
		std::lock_guard lock(s_mutex);
		return s_logged.size() < 64 && s_logged.insert(a_clip).second;
	}

	// ======================================================================
	// Per-frame player graph poll — faithful port of GunMover's GetAllClipInfo.
	//
	// GunMover never resolves clips from inside the Havok graph update; it
	// enumerates the PLAYER's graph manager once per frame from a hook that
	// runs OUTSIDE graph update, when activeNodes is complete and stable:
	//
	//     graphManager = player->currentProcess->middleHigh->animationGraphManager
	//     graph        = graphManager->variableCache.graphToCacheFor
	//     hkGraph      = *(graph + 0x378)
	//     skip if hkGraph->updateActiveNodes || hkGraph->stateOrTransitionChanged
	//     for each activeNodes entry:
	//         clip   = entry itself, or *(entry+0x08), whichever has the
	//                  hkbClipGenerator vtable            (SelectActiveClip)
	//         nested = *(entry+0x10)                      (ReadNestedBehaviorGraph)
	//         owner  = nested rootId if it is a BShkbAnimationGraph, else graph
	//         path   = swap-array match + file-array scan + resource probe
	//
	// We do the same from HookedActorUpdate (runs right after
	//     RunActorUpdatesOrig, i.e. after the Havok update cycle) and walk BOTH
	// player root graphs (3rd-person body and 1st-person) since OAR cares about
	// both. This gives us two things GunMover gets for free by starting from
	// the player:
	//   1. Ownership: every clip found here IS the player's (the hkbContext
	//      character is a static dummy in this runtime, so context-based actor
	//      attribution is impossible).
	//   2. Perspective: the root graph the clip lives in tells 1st vs 3rd
	//      person directly (learned via the first resolved _1stperson path).
	// ======================================================================
	// Direct path matching: re-key a clip's matching suffix to the exact suffix
	// of its resolved REAL path. At Activate time the subgraph walk usually
	// fails (the graph is mid-rebuild), so the cached suffix was derived from
	// the authored template name (e.g. "44pistol\wpnreload") and possibly
	// leaf-bridged; once the per-frame poll has resolved the true on-disk path,
	// the Update hook must match replacements against THAT path instead.
	// No-op when: the toggle is off, the suffix already matches, or one of OUR
	// replacements is currently installed in the clip's animation slot — the
	// restore/tracking state is keyed by the old suffix and re-keying
	// mid-replacement would desync it (the re-key happens on a later poll pass
	// once the replacement is uninstalled, or on the clip's next activation).
	// The clip's underlying ORIGINAL game animation (its binding identity),
	// regardless of whether one of our replacements is currently installed in
	// the animation slot. Null when the slot is empty (e.g. before the first
	// activate populates the control) or the original can't be recovered.
	static RE::hkaAnimation* GetBindingOriginalForClip(RE::hkbClipGenerator* a_clip)
	{
		if (!a_clip) return nullptr;
		{
			std::shared_lock lock(s_originalAnimMutex);
			auto it = s_originalAnimMap.find(a_clip);
			if (it != s_originalAnimMap.end() && it->second) return it->second;
		}
		auto** slot = a_clip->GetAnimationSlot();
		if (!slot || !*slot) return nullptr;
		auto* cache = AnimationCache::GetSingleton();
		if (cache->IsOurReplacement(*slot)) {
			return cache->GetOriginalFromReplacement(*slot);
		}
		return *slot;
	}

	// Record an authoritatively resolved REAL path against the clip's binding
	// identity (see s_bindingRealPath). Cheap no-op when already recorded.
	static void LearnBindingPathForClip(RE::hkbClipGenerator* a_clip, const std::string& a_realPath)
	{
		if (a_realPath.empty()) return;
		auto* bindingAnim = GetBindingOriginalForClip(a_clip);
		if (!bindingAnim) return;
		{
			std::shared_lock lock(s_bindingSuffixMutex);
			auto it = s_bindingRealPath.find(bindingAnim);
			if (it != s_bindingRealPath.end() && it->second == a_realPath) return;
		}
		std::unique_lock lock(s_bindingSuffixMutex);
		s_bindingRealPath[bindingAnim] = a_realPath;
	}

	// The real path learned for this clip's binding identity, or empty when
	// unknown. Leaf-validated: a recycled hkaAnimation address serving a
	// different animation must not inherit (the caller passes the leaf it
	// expects for this clip, e.g. from the authored suffix guess).
	static std::string InheritedBindingPathForClip(RE::hkbClipGenerator* a_clip, const std::string& a_expectedLeaf)
	{
		if (a_expectedLeaf.empty()) return {};
		auto* bindingAnim = GetBindingOriginalForClip(a_clip);
		if (!bindingAnim) return {};
		std::string path;
		{
			std::shared_lock lock(s_bindingSuffixMutex);
			auto it = s_bindingRealPath.find(bindingAnim);
			if (it != s_bindingRealPath.end()) path = it->second;
		}
		if (path.empty() || SubgraphGetLeaf(path.c_str()) != a_expectedLeaf) return {};
		return path;
	}

	// The clip's real registered-form suffix, best source first: the per-frame
	// poll's AUTHORITATIVE resolution (clip-keyed, refreshed while the clip is
	// live), then the leaf-validated binding inheritance. The inheritance
	// survives clip-pool recycling but is only leaf-validated — a freed
	// original's address reused by ANOTHER weapon's same-named animation
	// inherits the old weapon's path (field case: exact='hazord606\m4\...'
	// claimed on a different weapon's clip, 2026-08-16). Empty when unknown.
	static std::string RealSuffixForClip(RE::hkbClipGenerator* a_clip, const std::string& a_leafName)
	{
		bool authoritative = false;
		{
			std::shared_lock slock(s_clipRealPathStateMutex);
			authoritative = s_clipRealPathAuthoritative.contains(a_clip);
		}
		if (authoritative) {
			std::string cachedPath;
			{
				std::shared_lock plock(s_clipRealPathMutex);
				auto it = s_clipRealPathCache.find(a_clip);
				if (it != s_clipRealPathCache.end()) cachedPath = it->second;
			}
			if (!cachedPath.empty()) {
				auto sfx = ExtractAnimSuffix(cachedPath);
				std::string sfxLeaf = sfx;
				if (auto p = sfx.rfind('\\'); p != std::string::npos) sfxLeaf = sfx.substr(p + 1);
				if (!sfx.empty() && sfxLeaf == a_leafName) return sfx;
			}
		}
		const auto inh = InheritedBindingPathForClip(a_clip, a_leafName);
		if (!inh.empty()) return ExtractAnimSuffix(inh);
		return {};
	}

	// The clip's active-submod entry, validated against its CURRENT binding.
	// The engine recycles clip generators without firing Deactivate, so the
	// entry can describe an animation the clip no longer plays; honoring such
	// an entry as a non-interruptible lock skips condition evaluation and
	// re-registers the old weapon's submod onto the new weapon's clip (the
	// zombie track-filter state behind the 'stepped' blending, 2026-08-16).
	// A detected mismatch erases the entry and returns null. When either
	// binding is unknown the entry is honored (legacy behavior).
	static SubMod* ValidatedActiveSubMod(RE::hkbClipGenerator* a_clip)
	{
		RE::hkaAnimation* currentBinding = GetBindingOriginalForClip(a_clip);
		std::unique_lock smLock(s_activeSubModMutex);
		auto smIt = s_activeSubModMap.find(a_clip);
		if (smIt == s_activeSubModMap.end() || !smIt->second) return nullptr;
		auto bit = s_activeSubModBinding.find(a_clip);
		if (bit != s_activeSubModBinding.end() && bit->second && currentBinding &&
			bit->second != currentBinding) {
			static std::atomic<int> s_staleLockLog{ 0 };
			if (s_staleLockLog.fetch_add(1, std::memory_order_relaxed) < 20) {
				logger::info("[OAR] Dropping stale active-submod lock on clip {:X}: binding {:X} -> {:X} (submod '{}')",
					reinterpret_cast<uintptr_t>(a_clip),
					reinterpret_cast<uintptr_t>(bit->second),
					reinterpret_cast<uintptr_t>(currentBinding),
					smIt->second->GetName());
			}
			s_activeSubModMap.erase(smIt);
			s_activeSubModBinding.erase(bit);
			return nullptr;
		}
		return smIt->second;
	}

	static void EnsureDirectSuffixForClip(RE::hkbClipGenerator* a_clip, const std::string& a_realPath)
	{
		if (!Settings::GetSingleton()->bDirectPathMatching || a_realPath.empty()) return;

		const auto exactSuffix = ExtractAnimSuffix(a_realPath);
		if (exactSuffix.empty()) return;

		// Learn the binding identity BEFORE any early return below: the re-key
		// itself is deferred while our replacement occupies the slot, but the
		// resolution is authoritative NOW, and other clip instances of the
		// same binding need it to resolve without re-walking.
		LearnBindingPathForClip(a_clip, a_realPath);

		// The stored key must be override-aware: this cache feeds the matching
		// layer, which speaks "multi:<leaf>" for leaves claimed by a Leaf
		// Matching submod. Writing the exact form here flipped an already
		// claimed clip back to exact mid-play — the Update hook then saw
		// NoMatch and abandoned the replacement with its track-filter state
		// stranded (field case: mcxanims\wpnmelee, 2026-08-16). The binding
		// identity itself is learned exact above; only the matching key is
		// converted.
		const auto matchKey = ApplyLeafOverride(exactSuffix);

		{
			std::shared_lock lock(s_clipSuffixMutex);
			auto it = s_clipSuffixCache.find(a_clip);
			if (it != s_clipSuffixCache.end() && it->second == matchKey) return;
		}

		if (auto** slot = a_clip->GetAnimationSlot(); slot && *slot &&
			AnimationCache::GetSingleton()->IsOurReplacement(*slot)) {
			return;
		}

		std::string oldSuffix;
		{
			std::unique_lock lock(s_clipSuffixMutex);
			auto it = s_clipSuffixCache.find(a_clip);
			if (it != s_clipSuffixCache.end()) oldSuffix = it->second;
			s_clipSuffixCache[a_clip] = matchKey;
		}
		static std::atomic<int> s_rekeyLog{ 0 };
		if (s_rekeyLog.fetch_add(1, std::memory_order_relaxed) < 40) {
			logger::info("[OAR-DirectPath] Re-keyed clip {:X} suffix '{}' -> '{}' (real path '{}')",
				reinterpret_cast<uintptr_t>(a_clip), oldSuffix, matchKey, a_realPath);
		}
	}

	static void PollPlayerGraphClips()
	{
		constexpr uintptr_t kBShkb_HkRootGraph = 0x378;
		constexpr uintptr_t kBG_ActiveNodes = 0xE0;

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) return;
		RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
		if (!player->GetAnimationGraphManagerImpl(manager) || !manager) return;

		static REL::Relocation<uintptr_t> bshkbVtbl{ RE::VTABLE::BShkbAnimationGraph[0] };
		const auto clipVtbl = Offsets::hkbClipGenerator_vtbl.address();

		static std::atomic<int> s_pollDiagCount{ 0 };

		for (uint32_t gi = 0; gi < manager->graph.size() && gi < 4; ++gi) {
			const auto root = reinterpret_cast<uintptr_t>(manager->graph[gi].get());
			if (!root || root < 0x10000 ||
				IsBadReadPtr(reinterpret_cast<void*>(root), kBShkb_HkRootGraph + 8) ||
				*reinterpret_cast<uintptr_t*>(root) != bshkbVtbl.address()) {
				continue;
			}
			const auto hkGraph = *reinterpret_cast<uintptr_t*>(root + kBShkb_HkRootGraph);
			if (!hkGraph || hkGraph < 0x10000 ||
				IsBadReadPtr(reinterpret_cast<void*>(hkGraph), 0x1B0)) {
				continue;
			}

			// Opportunistic: learn the 1st-person root graph from the graph's
			// OWN project path ("Actors\Character\_1stPerson\..."). NOTE: in
			// this runtime the project animationPath is usually EMPTY (see
			// OAR-WeaponPath logs), so the primary learning signal is resolved
			// clip paths carrying a "1stperson" marker (below); this block is
			// a free extra chance in case some runtime/project provides it.
			if (s_firstPersonGraphIndex.load(std::memory_order_relaxed) < 0) {
				auto* character = reinterpret_cast<RE::hkbCharacter*>(root + kGraph_EmbeddedCharacter);
				if (!IsBadReadPtr(character, 0xB0)) {
					auto* projData = character->projectData._ptr;
					if (projData && !IsBadReadPtr(projData, 0x30)) {
						auto* projStrData = projData->stringData._ptr;
						if (projStrData && !IsBadReadPtr(projStrData, 0x80)) {
							const char* rawPath = projStrData->animationPath.data();
							if (rawPath && reinterpret_cast<uintptr_t>(rawPath) > 0x10000 &&
								!IsBadReadPtr(rawPath, 1) && rawPath[0] != '\0' &&
								ClassifyPerspectiveFromPath(rawPath) == AnimationLog::Perspective::kFirstPerson) {
								s_firstPersonGraphIndex.store(static_cast<int32_t>(gi), std::memory_order_relaxed);
								logger::info("[OAR-Poll] Player root graph [{}] identified as 1st-person (project path '{}')",
									gi, rawPath);
							}
						}
					}
				}
			}

			// GunMover: skip while the graph rebuilds its node list
			if (*reinterpret_cast<const uint8_t*>(hkGraph + 0x1AC) != 0 ||
				*reinterpret_cast<const uint8_t*>(hkGraph + 0x1AD) != 0) {
				continue;
			}
			const auto activeNodes = *reinterpret_cast<uintptr_t*>(hkGraph + kBG_ActiveNodes);
			if (!activeNodes || IsBadReadPtr(reinterpret_cast<void*>(activeNodes), 0x10)) {
				continue;
			}
			const auto data = *reinterpret_cast<uintptr_t*>(activeNodes);
			const auto size = *reinterpret_cast<int32_t*>(activeNodes + 8);
			if (!data || size <= 0 || size > 0x1000 ||
				IsBadReadPtr(reinterpret_cast<void*>(data), static_cast<size_t>(size) * sizeof(void*))) {
				continue;
			}

			for (int32_t i = 0; i < size; ++i) {
				const auto entry = *reinterpret_cast<uintptr_t*>(data + static_cast<uintptr_t>(i) * sizeof(void*));
				if (!entry || IsBadReadPtr(reinterpret_cast<void*>(entry), 0x18)) {
					continue;
				}

				// GunMover's SelectActiveClip: entry itself, or entry+0x08,
				// whichever carries the hkbClipGenerator vtable.
				uintptr_t clipAddr = 0;
				if (*reinterpret_cast<uintptr_t*>(entry) == clipVtbl) {
					clipAddr = entry;
				} else {
					const auto candidate = *reinterpret_cast<uintptr_t*>(entry + 0x08);
					if (candidate && candidate > 0x10000 &&
						!IsBadReadPtr(reinterpret_cast<void*>(candidate), sizeof(void*)) &&
						*reinterpret_cast<uintptr_t*>(candidate) == clipVtbl) {
						clipAddr = candidate;
					}
				}
				if (!clipAddr) continue;

				auto* clip = reinterpret_cast<RE::hkbClipGenerator*>(clipAddr);
				{
					std::unique_lock lock(s_playerClipMutex);
					s_playerClipGraph[clip] = static_cast<uint8_t>(gi);
				}

				// Already resolved authoritatively — just keep the matching suffix
				// keyed to the real path (a re-Activate may have overwritten it
				// with the authored/leaf-bridged suffix when the subgraph walk
				// failed at Activate time), then move on.
				{
					bool authoritative = false;
					{
						std::shared_lock slock(s_clipRealPathStateMutex);
						authoritative = s_clipRealPathAuthoritative.contains(clip);
					}
					if (authoritative) {
						std::string cachedPath;
						{
							std::shared_lock plock(s_clipRealPathMutex);
							auto pit = s_clipRealPathCache.find(clip);
							if (pit != s_clipRealPathCache.end()) cachedPath = pit->second;
						}
						EnsureDirectSuffixForClip(clip, cachedPath);
						continue;
					}
				}

				// Leaf from the clip's authored animation path; if the engine
				// already cleared it, use the authored path backfilled into the
				// display cache at Activate time.
				auto leaf = SubgraphGetLeaf(clip->animationName.data());
				if (leaf.empty()) {
					std::shared_lock plock(s_clipRealPathMutex);
					auto pit = s_clipRealPathCache.find(clip);
					if (pit != s_clipRealPathCache.end()) {
						leaf = SubgraphGetLeaf(pit->second.c_str());
					}
				}
				if (leaf.empty()) continue;

				// GunMover's resolution for this entry, with stage diagnostics
				const auto nested = *reinterpret_cast<uintptr_t*>(entry + 0x10);
				uintptr_t rootId = 0;
				if (nested && nested > 0x10000 &&
					!IsBadReadPtr(reinterpret_cast<void*>(nested), kBG_RootId + 8)) {
					rootId = *reinterpret_cast<uintptr_t*>(nested + kBG_RootId);
				}
				const auto owner = SubgraphResolveOwningGraph(nested, root);
				const auto dataBlock = rootId ? SubgraphFindSwapData(owner, rootId) : 0;
				std::string path;
				if (dataBlock) {
					path = SubgraphScanFileArrays(dataBlock, leaf);
				}

				// GunMover matches every clip against ONE owner graph — the
				// manager's main graph (variableCache.graphToCacheFor) — not
				// the root the clip was found in. The 1st-person root's own
				// swap array even produces FALSE matches for its clips (the
				// matched block's "file arrays" hold garbage bytes; verified
				// via OAR-PollDump). So when this clip's own (owner, rootId)
				// pair yields nothing, retry graphToCacheFor and the other
				// player root graphs as the owner. Safe against wrong-weapon
				// results: the match key is still THIS clip's nested-graph
				// root id, and only probe-verified (existing) paths are ever
				// accepted.
				if (path.empty() && rootId) {
					uintptr_t altOwners[5]{};
					size_t altCount = 0;
					if (auto* cacheGraph = manager->variableCache.graphToCacheFor.get()) {
						altOwners[altCount++] = reinterpret_cast<uintptr_t>(cacheGraph);
					}
					for (uint32_t gj = 0; gj < manager->graph.size() && gj < 4; ++gj) {
						altOwners[altCount++] = reinterpret_cast<uintptr_t>(manager->graph[gj].get());
					}
					for (size_t a = 0; a < altCount && path.empty(); ++a) {
						const auto altOwner = altOwners[a];
						if (!altOwner || altOwner == owner) continue;
						const auto altBlock = SubgraphFindSwapData(altOwner, rootId);
						if (!altBlock || altBlock == dataBlock) continue;
						path = SubgraphScanFileArrays(altBlock, leaf);
					}
				}

				if (!path.empty()) {
					{
						std::unique_lock plock(s_clipRealPathMutex);
						s_clipRealPathCache[clip] = path;
					}
					{
						std::unique_lock slock(s_clipRealPathStateMutex);
						s_clipRealPathAuthoritative.insert(clip);
						s_clipRealPathAttempts.erase(clip);
					}
					// Direct path matching: switch the clip's matching suffix to
					// the real path's exact suffix from now on (see the helper).
					EnsureDirectSuffixForClip(clip, path);
					// Learn which player root graph is the 1st-person one from
					// the first resolved path carrying the _1stperson marker.
					if (s_firstPersonGraphIndex.load(std::memory_order_relaxed) < 0 &&
						ClassifyPerspectiveFromPath(path) == AnimationLog::Perspective::kFirstPerson) {
						s_firstPersonGraphIndex.store(static_cast<int32_t>(gi), std::memory_order_relaxed);
						logger::info("[OAR-Poll] Player root graph [{}] identified as 1st-person (via '{}')", gi, path);
					}
					static std::atomic<int> s_pollResolveLog{ 0 };
					if (s_pollResolveLog.fetch_add(1) < 60) {
						logger::info("[OAR-Poll] graph[{}] clip='{}' -> '{}'", gi, leaf, path);
					}
				} else if (SubgraphShouldLogPollFailure(clip) && s_pollDiagCount.fetch_add(1) < 30) {
					// Stage diagnostics for the first failures (once per clip —
					// the same few clips repeat every frame): exactly which
					// stage broke (owner? swap match? file scan/probe?).
					logger::info(
						"[OAR-Poll] graph[{}] clip='{}' FAILED: nested={:X} rootId={:X} owner={:X}{} dataBlock={:X}{}",
						gi, leaf, nested, rootId, owner,
						owner == root ? " (fallback=root)" : "",
						dataBlock,
						dataBlock ? " (scan/probe found nothing)" : "");

					// When the swap match worked but the scan/probe failed, dump
					// the data block's file arrays once so we can see exactly
					// what the engine has selected (and why nothing probed true).
					if (dataBlock) {
						static std::mutex s_dumpMutex;
						static std::unordered_set<uintptr_t> s_dumpedBlocks;
						bool doDump = false;
						{
							std::lock_guard dlock(s_dumpMutex);
							doDump = s_dumpedBlocks.size() < 4 && s_dumpedBlocks.insert(dataBlock).second;
						}
						if (doDump) {
							const auto dumpArray = [&](uintptr_t a_off, const char* a_label) {
								const auto array = dataBlock + a_off;
								if (IsBadReadPtr(reinterpret_cast<void*>(array), 0x18)) {
									logger::info("[OAR-PollDump]   {} @+{:X}: unreadable", a_label, a_off);
									return;
								}
								const auto entries = *reinterpret_cast<uintptr_t*>(array);
								const auto size = *reinterpret_cast<uint32_t*>(array + 0x10);
								logger::info("[OAR-PollDump]   {} @+{:X}: entries={:X} size={}", a_label, a_off, entries, size);
								if (!entries || size == 0 || size > 0x400 ||
									IsBadReadPtr(reinterpret_cast<void*>(entries), size * sizeof(void*))) {
									return;
								}
								for (uint32_t k = 0; k < size && k < 16; ++k) {
									const auto strPtr = *reinterpret_cast<uintptr_t*>(entries + static_cast<uintptr_t>(k) * sizeof(void*));
									const char* s = SubgraphDecodePoolEntry(strPtr);
									if (!s) s = "(null)";
									// Show the candidate this entry yields for the failing leaf + probe result
									auto cand = SubgraphBuildCandidate(s, leaf);
									logger::info("[OAR-PollDump]     [{}] '{}' -> cand='{}' exists={}",
										k, s, cand, !cand.empty() && SubgraphResourceExists(cand));
								}
							};
							logger::info("[OAR-PollDump] dataBlock={:X} (for clip '{}')", dataBlock, leaf);
							dumpArray(kData_FileArrayPrimary, "primary");
							dumpArray(kData_FileArrayFallback, "fallback");
						}
					}
				}
			}
		}

	}

	// Player ownership test that works AT Activate time — unlike the per-frame
	// poll, which must skip a graph while it rebuilds its node list (exactly
	// when clips activate). The hkbContext's character is a static dummy in
	// this runtime, so instead we take the clip's hkbBehaviorGraph — from the
	// context (+0x08) and/or the clip's hkbNodeInfo (+0x10) — and test it
	// against each player root graph two ways:
	//   1. It IS the root graph's own hkbBehaviorGraph (+0x378): top-level clip.
	//   2. Its root id (+0x30) matches an entry in the root graph's subgraph
	//      swap array: subgraph (weapon) clip. The swap array is graph-load
	//      data, stable even while activeNodes rebuilds, and the root id is a
	//      pointer unique to this actor's subgraph instance — a match on the
	//      player's array can only mean a player-owned clip.
	// Returns the player root graph index, or -1 (NPCs/creatures, bad data).
	static int32_t PlayerGraphIndexForClip(RE::hkbClipGenerator* a_clip, const RE::hkbContext* a_context)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) return -1;
		RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
		if (!player->GetAnimationGraphManagerImpl(manager) || !manager) return -1;

		constexpr uintptr_t kBShkb_HkRootGraph = 0x378;  // BShkbAnimationGraph -> root hkbBehaviorGraph
		static REL::Relocation<uintptr_t> bshkbVtbl{ RE::VTABLE::BShkbAnimationGraph[0] };

		// Collect the clip's behavior-graph candidates (context first, then nodeInfo)
		uintptr_t candidates[2]{};
		size_t candidateCount = 0;
		if (a_context && reinterpret_cast<uintptr_t>(a_context) > 0x10000 &&
			!IsBadReadPtr(a_context, kCtx_BehaviorGraph + 8)) {
			const auto g = *reinterpret_cast<const uintptr_t*>(
				reinterpret_cast<uintptr_t>(a_context) + kCtx_BehaviorGraph);
			if (g && g > 0x10000) candidates[candidateCount++] = g;
		}
		if (a_clip && a_clip->nodeInfo &&
			reinterpret_cast<uintptr_t>(a_clip->nodeInfo) > 0x10000 &&
			!IsBadReadPtr(a_clip->nodeInfo, 0x18)) {
			const auto g = *reinterpret_cast<uintptr_t*>(
				reinterpret_cast<uintptr_t>(a_clip->nodeInfo) + 0x10);
			if (g && g > 0x10000 && g != candidates[0]) candidates[candidateCount++] = g;
		}
		if (candidateCount == 0) return -1;

		for (uint32_t i = 0; i < manager->graph.size() && i < 4; ++i) {
			const auto root = reinterpret_cast<uintptr_t>(manager->graph[i].get());
			if (!root || root < 0x10000 ||
				IsBadReadPtr(reinterpret_cast<void*>(root), kBShkb_HkRootGraph + 8) ||
				*reinterpret_cast<uintptr_t*>(root) != bshkbVtbl.address()) {
				continue;
			}
			const auto rootHkGraph = *reinterpret_cast<uintptr_t*>(root + kBShkb_HkRootGraph);

			for (size_t c = 0; c < candidateCount; ++c) {
				const auto nested = candidates[c];
				// Test 1: top-level clip — the candidate is the root's own graph
				if (rootHkGraph && nested == rootHkGraph) {
					return static_cast<int32_t>(i);
				}
				// Test 2: subgraph clip — its root id is registered in this
				// root's swap array (or points back at the root itself)
				if (IsBadReadPtr(reinterpret_cast<void*>(nested), kBG_RootId + 8)) {
					continue;
				}
				const auto rootId = *reinterpret_cast<uintptr_t*>(nested + kBG_RootId);
				if (!rootId) continue;
				if (rootId == root || SubgraphFindSwapData(root, rootId) != 0) {
					return static_cast<int32_t>(i);
				}
			}
		}
		return -1;
	}

	// Actor attribution for anim-log entries. Player-graph membership (from the
	// per-frame poll or from Activate-time context matching) is the primary
	// player test; the embedded-character graph comparison covers clips the
	// poll hasn't seen yet. Non-player clips fall back to the character cache
	// and may legitimately return nullptr ("Unknown" in the log) — never the
	// player.
	static RE::TESObjectREFR* ResolveLogRefr(RE::hkbClipGenerator* a_clip, const RE::hkbContext* a_context)
	{
		{
			std::shared_lock lock(s_playerClipMutex);
			if (s_playerClipGraph.contains(a_clip)) {
				return RE::PlayerCharacter::GetSingleton();
			}
		}
		// Membership can be missing when the poll hasn't seen this clip yet
		// (graphs are skipped mid-rebuild — exactly when clips activate).
		// Derive ownership from the clip's behavior graph instead and
		// backfill the membership map so perspective classification works too.
		if (const auto gi = PlayerGraphIndexForClip(a_clip, a_context); gi >= 0) {
			{
				std::unique_lock lock(s_playerClipMutex);
				s_playerClipGraph[a_clip] = static_cast<uint8_t>(gi);
			}
			return RE::PlayerCharacter::GetSingleton();
		}
		return GetRefrFromContext(a_context);
	}

	// Display path for a log entry, validated against the entry's animation.
	// A clip generator address can be recycled or rebound to a different
	// animation between an entry's creation (Activate) and its deferred flush
	// (Update/Deactivate) — and the per-frame poll caches the path of whatever
	// the clip is playing NOW. Blindly attaching the cached path produced
	// entries like name 'idle' with path '...\SCAR\wpnpitchupreadyadd.hkx'.
	// Real paths are built as <selected dir>\<authored leaf>.hkx, so the leaf
	// must match the entry's suffix leaf; reject the path otherwise.
	static std::string DisplayPathForEntry(RE::hkbClipGenerator* a_clip, const std::string& a_suffix)
	{
		std::string path;
		{
			std::shared_lock plock(s_clipRealPathMutex);
			auto it = s_clipRealPathCache.find(a_clip);
			if (it != s_clipRealPathCache.end()) path = it->second;
		}
		if (path.empty()) return path;

		// Suffix forms: "dir\leaf", "leaf", or "multi:leaf"
		std::string suffix = a_suffix;
		if (suffix.rfind("multi:", 0) == 0) suffix = suffix.substr(6);
		const auto suffixLeaf = SubgraphGetLeaf(suffix.c_str());
		if (suffixLeaf.empty() || SubgraphGetLeaf(path.c_str()) != suffixLeaf) {
			return {};
		}
		return path;
	}

	// Perspective for anim-log entries. Player-graph membership is authoritative
	// when the 1st-person graph index has been learned; otherwise fall back to
	// the path marker rule (contains "_1stperson" => 1st person).
	static AnimationLog::Perspective ClassifyClipPerspective(RE::hkbClipGenerator* a_clip, const std::string& a_path)
	{
		{
			std::shared_lock lock(s_playerClipMutex);
			auto it = s_playerClipGraph.find(a_clip);
			if (it != s_playerClipGraph.end()) {
				const auto fpIdx = s_firstPersonGraphIndex.load(std::memory_order_relaxed);
				if (fpIdx >= 0) {
					return it->second == static_cast<uint8_t>(fpIdx) ?
						AnimationLog::Perspective::kFirstPerson :
						AnimationLog::Perspective::kThirdPerson;
				}
			}
		}
		return ClassifyPerspectiveFromPath(a_path);
	}

	OARClipPerspective GetPlayingClipPerspectiveImpl(RE::hkbClipGenerator* a_clip)
	{
		if (!a_clip) return OARClipPerspective::kUnknown;

		// The player runs both perspective graphs at the same time. Membership
		// learned by the graph poll is authoritative and avoids treating every
		// player clip as whichever camera happens to be visible this frame.
		{
			std::shared_lock lock(s_playerClipMutex);
			auto it = s_playerClipGraph.find(a_clip);
			if (it != s_playerClipGraph.end()) {
				const auto firstPersonIndex = s_firstPersonGraphIndex.load(std::memory_order_relaxed);
				if (firstPersonIndex >= 0) {
					return it->second == static_cast<uint8_t>(firstPersonIndex) ?
						OARClipPerspective::kFirstPerson : OARClipPerspective::kThirdPerson;
				}
			}
		}

		// Non-player clips and early player clips can still be classified from
		// the authoritative resolved path. Do not label an empty/unknown player
		// path as third person merely because it lacks the _1stPerson marker.
		std::string resolvedPath;
		{
			std::shared_lock lock(s_clipRealPathMutex);
			auto it = s_clipRealPathCache.find(a_clip);
			if (it != s_clipRealPathCache.end()) resolvedPath = it->second;
		}
		if (resolvedPath.empty()) return OARClipPerspective::kUnknown;

		return ClassifyPerspectiveFromPath(resolvedPath) == AnimationLog::Perspective::kFirstPerson ?
			OARClipPerspective::kFirstPerson : OARClipPerspective::kThirdPerson;
	}

	// Track filters deliberately paste their cached pose into other clips that
	// the same graph blends alongside the source. Named additive support clips
	// are different: they carry sway, pitch, turn, jiggle, and stance deltas.
	// Pasting the absolute cached pose into them, or zeroing their filtered
	// tracks, can corrupt the composed camera/weapon hierarchy. The two config
	// switches are perspective-specific because the player's graphs run at the
	// same time. Unknown perspective retains the historical behavior.
	static bool ShouldSkipAddNonSourceClip(
		RE::hkbClipGenerator* a_clip, const SubMod::TrackFilter* a_filter)
	{
		if (!a_clip || !a_filter) return false;

		std::string suffix;
		{
			std::shared_lock lock(s_clipSuffixMutex);
			auto it = s_clipSuffixCache.find(a_clip);
			if (it != s_clipSuffixCache.end()) suffix = it->second;
		}
		if (suffix.empty()) {
			const char* clipName = a_clip->animationName.data();
			if (clipName && reinterpret_cast<uintptr_t>(clipName) > 0x10000 && clipName[0] != '\0') {
				suffix = ExtractAnimSuffix(std::string(clipName));
			}
		}
		if (suffix.empty() || !LeafEndsWithAdd(GetSuffixLeaf(suffix))) return false;

		const auto perspective = GetPlayingClipPerspectiveImpl(a_clip);
		const bool skip =
			(perspective == OARClipPerspective::kFirstPerson &&
				a_filter->skipAdditiveNonSourceFirstPerson) ||
			(perspective == OARClipPerspective::kThirdPerson &&
				a_filter->skipAdditiveNonSourceThirdPerson);
		if (skip) {
			static std::atomic<int> s_skipAddLog{ 0 };
			if (s_skipAddLog.fetch_add(1, std::memory_order_relaxed) < 40) {
				logger::info(
					"[OAR-TrackFilter] Skipped non-source *add clip '{}' ({}, clip={:X})",
					suffix,
					perspective == OARClipPerspective::kFirstPerson ? "1st person" : "3rd person",
					reinterpret_cast<uintptr_t>(a_clip));
			}
		}
		return skip;
	}

	// Skeleton-root signature of an animation path: the lowercased segment
	// between 'actors\' and '\animations\', e.g. 'character\_1stperson',
	// 'character', 'powerarmor\_1stperson', 'powerarmor', a creature race.
	// This is the discriminator suffix matching throws away: suffixes are
	// computed after 'Animations\', so a power-armor clip's suffix collides
	// with the normal-skeleton registration even though the skeletons differ.
	// Empty = unknown (path unresolved or not under an actors tree).
	static std::string AnimRootSignature(const std::string& a_path)
	{
		if (a_path.empty()) return {};
		std::string lower = a_path;
		std::ranges::transform(lower, lower.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::ranges::replace(lower, '/', '\\');
		const auto actorsPos = lower.find("actors\\");
		if (actorsPos == std::string::npos) return {};
		const auto animPos = lower.find("\\animations\\", actorsPos);
		if (animPos == std::string::npos || animPos <= actorsPos + 7) return {};
		return lower.substr(actorsPos + 7, animPos - (actorsPos + 7));
	}

	// Skeleton gate for ALL candidate selection: a replacement authored for
	// one skeleton must never install on a clip playing on another — the
	// donor's track->bone indices are meaningless there and the result is a
	// corrupted pose. Two field cases define the class: the 3rd-person
	// F4Parkour 'rifle\wpnmelee' claiming 1st-person melee clips via leaf
	// matching (Cryolator, 2026-08-19), and a tactical-reload submod firing
	// on the POWER ARMOR reload clip (different skeleton, same suffix — the
	// root segment is what suffix matching discards). Compare root
	// signatures when both sides resolve; fall back to the 1st/3rd-person
	// classifier (player graph index works even before the clip's path
	// resolves); allow when genuinely unknown (NPC/unresolved paths keep
	// today's behavior).
	static bool ClaimSkeletonAllowed(RE::hkbClipGenerator* a_clip, const ReplacementAnimFileInfo* a_info)
	{
		if (!a_info) return false;
		// This protection is deliberately opt-in. A special idle may use a
		// third-person-authored animation while hosted by the player's first-person
		// behavior graph, so graph perspective alone can otherwise reject a valid
		// exact-path replacement (F4Parkour Ledge/Mantle High).
		if (!Settings::GetSingleton()->bSkeletonCompatibilityGate) return true;
		const std::string& filePath =
			!a_info->absoluteDiskPath.empty() ? a_info->absoluteDiskPath : a_info->replacementPath;
		const std::string fileSig = AnimRootSignature(filePath);
		std::string clipPath;
		{
			std::shared_lock lock(s_clipRealPathMutex);
			auto it = s_clipRealPathCache.find(a_clip);
			if (it != s_clipRealPathCache.end()) clipPath = it->second;
		}
		const std::string clipSig = AnimRootSignature(clipPath);
		if (!fileSig.empty() && !clipSig.empty()) {
			if (fileSig == clipSig) return true;
			static std::atomic<int> s_skelGateLog{ 0 };
			if (s_skelGateLog.fetch_add(1, std::memory_order_relaxed) < 30) {
				logger::info("[OAR-SkelGate] blocked candidate: file root='{}' clip root='{}' (clipGen={:X}, file='{}')",
					fileSig, clipSig, reinterpret_cast<uintptr_t>(a_clip), a_info->replacementPath);
			}
			return false;
		}
		// Root unknown on one side: the perspective classifier still separates
		// 1st from 3rd person via the player graph index.
		using Perspective = AnimationLog::Perspective;
		const auto filePersp = ClassifyPerspectiveFromPath(filePath);
		if (filePersp == Perspective::kUnknown) return true;
		const auto clipPersp = ClassifyClipPerspective(a_clip, clipPath);
		if (clipPersp == Perspective::kUnknown) return true;
		return clipPersp == filePersp;
	}

	// Direct path matching: the exact suffix from a clip's previously resolved
	// (authoritative) real path. Leaf-validated against the clip's current
	// authored animation name — a recycled clip-generator address can carry a
	// stale cache entry for a different animation. Returns empty when the
	// toggle is off or no validated path is available.
	static std::string DirectSuffixFromCachedPath(RE::hkbClipGenerator* a_clip)
	{
		if (!Settings::GetSingleton()->bDirectPathMatching) return {};
		{
			std::shared_lock slock(s_clipRealPathStateMutex);
			if (!s_clipRealPathAuthoritative.contains(a_clip)) return {};
		}
		std::string cachedPath;
		{
			std::shared_lock plock(s_clipRealPathMutex);
			auto it = s_clipRealPathCache.find(a_clip);
			if (it != s_clipRealPathCache.end()) cachedPath = it->second;
		}
		if (cachedPath.empty()) return {};
		const auto clipLeaf = SubgraphGetLeaf(a_clip->animationName.data());
		if (clipLeaf.empty() || SubgraphGetLeaf(cachedPath.c_str()) != clipLeaf) return {};
		// Even the resolved real path yields to a Leaf Matching submod: the
		// override converts to multi mode, where the flagged submod is probed
		// first and path candidates remain the fallback.
		return ApplyLeafOverride(ExtractAnimSuffix(cachedPath));
	}

	static std::string GetClipSuffixFromContext(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context)
	{
		static int s_sourceLogCount = 0;
		static int s_diagLogCount = 0;

		// ===== Source S: Selected-subgraph swap array (deterministic real path) =====
		// When this resolves, it is the engine's own ground truth for which file
		// the clip is playing — it takes precedence over all heuristic sources.
		{
			const auto realPath = ResolveClipPathFromSubgraph(a_this, a_context);
			if (!realPath.empty()) {
				// Remember the authoritative on-disk path for this clip so the
				// Animation Log can display it in full (display-only cache).
				{
					std::unique_lock plock(s_clipRealPathMutex);
					s_clipRealPathCache[a_this] = realPath;
				}
				{
					std::unique_lock slock(s_clipRealPathStateMutex);
					s_clipRealPathAuthoritative.insert(a_this);
				}
				auto suffix = ExtractAnimSuffix(realPath);
				if (!suffix.empty()) {
					// NOTE: deliberately NOT learning the binding identity here.
					// This runs at Activate time, where a recycled clip's slot
					// can still hold the PREVIOUS clip's animation — the learn
					// happens at Update/poll time via EnsureDirectSuffixForClip.
					bool registered;
					{
						std::shared_lock rlock(s_nameLookupMutex);
						registered = s_suffixToInfos.find(suffix) != s_suffixToInfos.end();
					}
					if (s_sourceLogCount < 80) {
						logger::info("[OAR-Suffix] SourceS-Subgraph: realPath='{}' -> suffix='{}' registered={}",
							realPath, suffix, registered);
						s_sourceLogCount++;
					}
					if (registered) {
						return ApplyLeafOverride(suffix);
					}
					// Not registered under the exact real-path suffix.
					// Direct path matching (default): the real path is the engine's
					// ground truth — if no replacement is registered under it, there
					// is NO replacement for this clip. Leaf matching stays available
					// only as a fallback for clips whose real path can't be resolved
					// (it never reaches here in that case — Source S failed).
					if (Settings::GetSingleton()->bDirectPathMatching) {
						return ApplyLeafOverride(suffix);
					}
					// Legacy behavior: bridge through the leaf table so replacer
					// layouts that only match by leaf name keep working.
					auto resolved = ResolveOrLeafFallback(suffix);
					if (resolved != suffix && s_sourceLogCount < 80) {
						logger::info("[OAR-Suffix] SourceS-Subgraph: leaf-bridged '{}' -> '{}'",
							suffix, resolved);
						s_sourceLogCount++;
					}
					return resolved;
				}
			}
		}

		// ===== Source S': previously resolved real path (per-frame player poll) =====
		// The fresh subgraph walk above usually FAILS at Activate time (the graph
		// is mid-rebuild), but the per-frame poll may have already resolved this
		// clip's real path on an earlier frame. With direct path matching enabled,
		// reuse it so a re-Activate doesn't silently fall back to the authored
		// template name (and through it, to leaf matching). Leaf-validated: a
		// recycled clip-generator address can carry a stale cache entry for a
		// different animation — only trust the path when its leaf matches the
		// clip's current authored leaf.
		{
			auto suffix = DirectSuffixFromCachedPath(a_this);
			if (!suffix.empty()) {
				if (s_sourceLogCount < 80) {
					logger::info("[OAR-Suffix] SourceS-Cached: suffix='{}' (poll-resolved real path)", suffix);
					s_sourceLogCount++;
				}
				return suffix;
			}
		}

		// ===== Source 0: Weapon graph animationPath + clip leaf name =====
		// Uses the REAL weapon character from PlayerCharacter->graphManager (Options 1+2+3)
		// This is the primary resolution path for weapon animations.
		{
			const char* clipName = a_this->animationName.data();
			if (clipName && reinterpret_cast<uintptr_t>(clipName) > 0x10000 && clipName[0] != '\0') {
				std::string clipStr(clipName);
				std::ranges::transform(clipStr, clipStr.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::ranges::replace(clipStr, '/', '\\');

				// Extract leaf name from clip (e.g. "44pistol\wpnreload" -> "wpnreload")
				std::string clipLeaf = clipStr;
				auto clipSlash = clipStr.rfind('\\');
				if (clipSlash != std::string::npos) {
					clipLeaf = clipStr.substr(clipSlash + 1);
				}
				// Remove extension if present
				auto dotPos = clipLeaf.rfind('.');
				if (dotPos != std::string::npos) clipLeaf = clipLeaf.substr(0, dotPos);

				if (!clipLeaf.empty()) {
					// Option 1: Use weapon graph's projectData animationPath
					if (s_weaponAnimFolderValid.load()) {
						std::shared_lock gLock(s_graphAnimPathMutex);
						if (!s_weaponAnimFolder.empty()) {
							std::string candidateSuffix = s_weaponAnimFolder + "\\" + clipLeaf;
							gLock.unlock();

							std::shared_lock rlock(s_nameLookupMutex);
							if (s_suffixToInfos.find(candidateSuffix) != s_suffixToInfos.end()) {
								if (s_sourceLogCount < 40) {
									logger::info("[OAR-Suffix] Source0-WeaponGraph: folder='{}' + leaf='{}' -> suffix='{}' (MATCH)",
										s_weaponAnimFolder, clipLeaf, candidateSuffix);
									s_sourceLogCount++;
								}
								return candidateSuffix;
							}
							if (s_sourceLogCount < 40) {
								logger::info("[OAR-Suffix] Source0-WeaponGraph: tried '{}' (no match in registry)",
									candidateSuffix);
								s_sourceLogCount++;
							}
						}
					}

					// Option 2: Use CreateFileW captured paths
					{
						std::shared_lock capLock(s_createFileAnimMutex);
						// First try the latest folder seen for this leaf
						auto latestIt = s_createFileLeafToLatestFolder.find(clipLeaf);
						if (latestIt != s_createFileLeafToLatestFolder.end()) {
							std::string candidateSuffix = latestIt->second + "\\" + clipLeaf;
							// Check if this exact captured path has a registered replacement
							std::shared_lock rlock(s_nameLookupMutex);
							if (s_suffixToInfos.find(candidateSuffix) != s_suffixToInfos.end()) {
								if (s_sourceLogCount < 40) {
									logger::info("[OAR-Suffix] Source0-CreateFile: latest folder='{}' + leaf='{}' -> suffix='{}' (MATCH)",
										latestIt->second, clipLeaf, candidateSuffix);
									s_sourceLogCount++;
								}
								return candidateSuffix;
							}
						}

						// Try all known folders for this leaf
						auto foldersIt = s_createFileLeafToFolders.find(clipLeaf);
						if (foldersIt != s_createFileLeafToFolders.end()) {
							for (auto& folder : foldersIt->second) {
								std::string candidateSuffix = folder + "\\" + clipLeaf;
								std::shared_lock rlock(s_nameLookupMutex);
								if (s_suffixToInfos.find(candidateSuffix) != s_suffixToInfos.end()) {
									if (s_sourceLogCount < 40) {
										logger::info("[OAR-Suffix] Source0-CreateFile: folder='{}' + leaf='{}' -> suffix='{}' (MATCH from set)",
											folder, clipLeaf, candidateSuffix);
										s_sourceLogCount++;
									}
									return candidateSuffix;
								}
							}
						}
					}

					// Option 3: Use subgraphID-to-folder mapping
					{
						uint64_t curSubID = s_lastWeaponSubGraphID.load();
						if (curSubID != 0) {
							std::shared_lock sgLock(s_subGraphToFolderMutex);
							auto sgIt = s_subGraphIDToFolder.find(curSubID);
							if (sgIt != s_subGraphIDToFolder.end()) {
								std::string candidateSuffix = sgIt->second + "\\" + clipLeaf;
								std::shared_lock rlock(s_nameLookupMutex);
								if (s_suffixToInfos.find(candidateSuffix) != s_suffixToInfos.end()) {
									if (s_sourceLogCount < 40) {
										logger::info("[OAR-Suffix] Source0-SubGraphID: id={:X} folder='{}' + leaf='{}' -> suffix='{}' (MATCH)",
											curSubID, sgIt->second, clipLeaf, candidateSuffix);
										s_sourceLogCount++;
									}
									return candidateSuffix;
								}
							}
						}
					}
				}
			}
		}

		// Source 1: Read animationNames[bindIdx].fileName from character->setup->data->stringData
		// Optionally prepend animationPath from projectData or LoadClips path map.
		if (a_context && reinterpret_cast<uintptr_t>(a_context) > 0x10000 &&
			!IsBadReadPtr(a_context, sizeof(void*)) &&
			a_context->character && reinterpret_cast<uintptr_t>(a_context->character) > 0x10000 &&
			!IsBadReadPtr(a_context->character, sizeof(void*)))
		{
			auto* character = a_context->character;

			// Detailed diagnostic logging (first N calls)
			bool logDiag = (s_diagLogCount < 10);
			if (logDiag) s_diagLogCount++;

			// Path A: try projectData -> stringData -> animationPath
			std::string animPath;
			auto* projData = character->projectData._ptr;
			if (logDiag) {
				logger::info("[OAR-Diag] character={:X} name='{}' setup={:X} projectData={:X} behaviorGraph={:X}",
					reinterpret_cast<uintptr_t>(character),
					(character->name.data() && !IsBadReadPtr(character->name.data(), 1)) ? character->name.data() : "(null)",
					reinterpret_cast<uintptr_t>(character->setup._ptr),
					reinterpret_cast<uintptr_t>(projData),
					reinterpret_cast<uintptr_t>(character->behaviorGraph._ptr));
			}
			if (projData && reinterpret_cast<uintptr_t>(projData) > 0x10000 &&
				!IsBadReadPtr(projData, 0x30)) {
				auto* projStrData = projData->stringData._ptr;
				if (logDiag) {
					logger::info("[OAR-Diag]   projData->stringData={:X}", reinterpret_cast<uintptr_t>(projStrData));
				}
				if (projStrData && reinterpret_cast<uintptr_t>(projStrData) > 0x10000 &&
					!IsBadReadPtr(projStrData, 0x80)) {
					const char* rawAnimPath = projStrData->animationPath.data();
					if (logDiag) {
						logger::info("[OAR-Diag]   projStrData->animationPath raw={:X} val='{}'",
							reinterpret_cast<uintptr_t>(rawAnimPath),
							(rawAnimPath && reinterpret_cast<uintptr_t>(rawAnimPath) > 0x10000 && !IsBadReadPtr(rawAnimPath, 1)) ? rawAnimPath : "(bad)");
					}
					if (rawAnimPath && reinterpret_cast<uintptr_t>(rawAnimPath) > 0x10000 &&
						!IsBadReadPtr(rawAnimPath, 1) && rawAnimPath[0] != '\0')
					{
						animPath = std::string(rawAnimPath);
					}
				}
			}

			// Path B: setup -> data -> characterStringData -> animationNames[bindIdx].fileName
			auto* setup = character->setup._ptr;
			if (logDiag && (!setup || reinterpret_cast<uintptr_t>(setup) < 0x10000)) {
				logger::info("[OAR-Diag]   setup is NULL/invalid ({:X})", reinterpret_cast<uintptr_t>(setup));
			}
			if (setup && reinterpret_cast<uintptr_t>(setup) > 0x10000 &&
				!IsBadReadPtr(setup, 0x50)) {
				auto* data = *reinterpret_cast<RE::hkbCharacterData**>(reinterpret_cast<uint8_t*>(setup) + 0x40);
				if (logDiag) {
					logger::info("[OAR-Diag]   setup->data(+0x40)={:X}", reinterpret_cast<uintptr_t>(data));
				}
				if (data && reinterpret_cast<uintptr_t>(data) > 0x10000 &&
					!IsBadReadPtr(data, 0xC0)) {
					auto* stringData = *reinterpret_cast<RE::hkbCharacterStringData**>(reinterpret_cast<uint8_t*>(data) + 0xB0);
					if (logDiag) {
						logger::info("[OAR-Diag]   data->stringData(+0xB0)={:X}", reinterpret_cast<uintptr_t>(stringData));
					}
					if (stringData && reinterpret_cast<uintptr_t>(stringData) > 0x10000 &&
						!IsBadReadPtr(stringData, 0x40)) {
						// Source 1c: Check if this stringData is in our LoadClips path map
						if (animPath.empty()) {
							std::shared_lock lock(s_loadClipsPathMutex);
							auto it = s_loadClipsPathMap.find(stringData);
							if (it != s_loadClipsPathMap.end()) {
								animPath = it->second;
								if (logDiag) {
									logger::info("[OAR-Diag]   LoadClipsMap hit! animPath='{}'", animPath);
								}
							}
						}

						int16_t bindIdx = a_this->animationBindingIndex;
						auto& animNames = stringData->animationNames;
						auto* arrBase = reinterpret_cast<const uint8_t*>(&animNames);
						auto* nameData = *reinterpret_cast<RE::hkbCharacterStringData::FileNameMeshNamePair* const*>(arrBase);
						int32_t nameSize = *reinterpret_cast<const int32_t*>(arrBase + 8);

						if (logDiag) {
							logger::info("[OAR-Diag]   animNames: data={:X} size={} bindIdx={}",
								reinterpret_cast<uintptr_t>(nameData), nameSize, bindIdx);
						}

						if (nameData && !IsBadReadPtr(nameData, sizeof(void*)) &&
							bindIdx >= 0 && bindIdx < nameSize)
						{
							const char* fileName = nameData[bindIdx].fileName.data();
							if (logDiag) {
								logger::info("[OAR-Diag]   animNames[{}].fileName='{}'", bindIdx,
									(fileName && reinterpret_cast<uintptr_t>(fileName) > 0x10000 && !IsBadReadPtr(fileName, 1)) ? fileName : "(bad)");
							}
							if (fileName && reinterpret_cast<uintptr_t>(fileName) > 0x10000 && fileName[0] != '\0') {
								if (!animPath.empty()) {
									std::string fullPath = animPath + fileName;
									auto suffix = ExtractAnimSuffix(fullPath);
									if (!suffix.empty()) {
										auto resolved = ResolveOrLeafFallback(suffix);
										if (s_sourceLogCount < 40) {
											logger::info("[OAR-Suffix] Source1-Combined: animPath='{}' + fileName='{}' -> '{}' resolved='{}'",
												animPath, fileName, suffix, resolved);
											s_sourceLogCount++;
										}
										return resolved;
									}
								}
								auto suffix = ExtractAnimSuffix(std::string(fileName));
								if (!suffix.empty()) {
									auto resolved = ResolveOrLeafFallback(suffix);
									if (s_sourceLogCount < 40) {
										logger::info("[OAR-Suffix] Source1b-FileName: fileName='{}' -> '{}' resolved='{}'",
											fileName, suffix, resolved);
										s_sourceLogCount++;
									}
									return resolved;
								}
							}
						}
					}
				}
			}
		}

		// Source 1d: Search all known stringData objects from player graphs
		// This bypasses the broken a_context->character chain entirely.
		// If the cache is empty, populate it on-demand from the player's graph manager.
		{
			{
				std::shared_lock check(s_knownStringDataMutex);
				if (s_knownStringDataList.empty()) {
					check.unlock();
					PopulateKnownStringData();
				}
			}
			int16_t bindIdx = a_this->animationBindingIndex;
			if (bindIdx >= 0) {
				std::shared_lock lock(s_knownStringDataMutex);
				for (auto* sd : s_knownStringDataList) {
					if (!sd || IsBadReadPtr(sd, 0x40)) continue;

					auto& animNames = sd->animationNames;
					auto* arrBase = reinterpret_cast<const uint8_t*>(&animNames);
					auto* nameData = *reinterpret_cast<RE::hkbCharacterStringData::FileNameMeshNamePair* const*>(arrBase);
					int32_t nameSize = *reinterpret_cast<const int32_t*>(arrBase + 8);

					if (!nameData || IsBadReadPtr(nameData, sizeof(void*)) || bindIdx >= nameSize)
						continue;

					const char* fileName = nameData[bindIdx].fileName.data();
					if (!fileName || reinterpret_cast<uintptr_t>(fileName) < 0x10000 ||
						IsBadReadPtr(fileName, 1) || fileName[0] == '\0')
						continue;

					// Validate: the leaf name of fileName should match the leaf of animationName
					const char* clipName = a_this->animationName.data();
					if (clipName && reinterpret_cast<uintptr_t>(clipName) > 0x10000) {
						std::string clipStr(clipName);
						std::string fileStr(fileName);
						// Extract leaf from both and compare
						auto clipLeaf = clipStr;
						auto clipSlash = clipStr.rfind('\\');
						if (clipSlash != std::string::npos) clipLeaf = clipStr.substr(clipSlash + 1);
						auto fileLeaf = fileStr;
						auto fileSlash = fileStr.rfind('\\');
						if (fileSlash != std::string::npos) fileLeaf = fileStr.substr(fileSlash + 1);
						// Remove .hkx/.hkt extension from fileLeaf for comparison
						auto dotPos = fileLeaf.rfind('.');
						if (dotPos != std::string::npos) fileLeaf = fileLeaf.substr(0, dotPos);

						// Normalize both to lowercase for comparison
						std::transform(clipLeaf.begin(), clipLeaf.end(), clipLeaf.begin(), ::tolower);
						std::transform(fileLeaf.begin(), fileLeaf.end(), fileLeaf.begin(), ::tolower);

						if (clipLeaf != fileLeaf) continue;
					}

					// Check LoadClips path map for prefix
					std::string animPath;
					{
						std::shared_lock plock(s_loadClipsPathMutex);
						auto it = s_loadClipsPathMap.find(sd);
						if (it != s_loadClipsPathMap.end()) {
							animPath = it->second;
						}
					}

					if (!animPath.empty()) {
						std::string fullPath = animPath + fileName;
						auto suffix = ExtractAnimSuffix(fullPath);
						if (!suffix.empty()) {
							auto resolved = ResolveOrLeafFallback(suffix);
							if (s_sourceLogCount < 40) {
								logger::info("[OAR-Suffix] Source1d-KnownSD: animPath='{}' + fileName='{}' -> '{}' resolved='{}'",
									animPath, fileName, suffix, resolved);
								s_sourceLogCount++;
							}
							return resolved;
						}
					}

					auto suffix = ExtractAnimSuffix(std::string(fileName));
					if (!suffix.empty()) {
						auto resolved = ResolveOrLeafFallback(suffix);
						if (s_sourceLogCount < 40) {
							logger::info("[OAR-Suffix] Source1d-KnownSD: fileName='{}' -> '{}' resolved='{}'",
								fileName, suffix, resolved);
							s_sourceLogCount++;
						}
						return resolved;
					}
				}
			}
		}

		// Source 2: LoadedIdleAnimData reverse lookup (real file -> clipGenerator)
		if (s_idleAnimReverseBuilt.load()) {
			std::shared_lock lock(s_idleAnimReverseMutex);
			auto it = s_idleAnimReverseMap.find(a_this);
			if (it != s_idleAnimReverseMap.end()) {
				auto suffix = ExtractAnimSuffix(it->second);
				if (!suffix.empty()) {
					auto resolved = ResolveOrLeafFallback(suffix);
					if (s_sourceLogCount < 20) {
						logger::info("[OAR-Suffix] Source2: idleAnimData='{}' -> '{}' resolved='{}'",
							it->second, suffix, resolved);
						s_sourceLogCount++;
					}
					return resolved;
				}
			}
		}

		// Source 3: animationName field (behavior template name, e.g. "44pistol\wpnreload")
		// Uses ResolveOrLeafFallback to handle multi-leaf matching automatically.
		const char* clipName = a_this->animationName.data();
		if (clipName && reinterpret_cast<uintptr_t>(clipName) > 0x10000 && clipName[0] != '\0') {
			std::string clipStr(clipName);
			auto fullSuffix = ExtractAnimSuffix(clipStr);

			if (!fullSuffix.empty()) {
				auto resolved = ResolveOrLeafFallback(fullSuffix);
				if (s_sourceLogCount < 40) {
					logger::info("[OAR-Suffix] Source3: animationName='{}' -> '{}' resolved='{}'",
						clipName, fullSuffix, resolved);
					s_sourceLogCount++;
				}
				return resolved;
			}
		}

		return {};
	}

	// Evaluate candidates in priority order (the lookup vectors are pre-sorted
	// highest priority first) and return the first whose SubMod is enabled and
	// whose conditions pass — an empty condition set passes unconditionally.
	// Returns nullptr when nothing matches. Mirrors the Update hook's winner
	// selection loop so the Activate-time pre-swap picks the SAME file that
	// Update will install, instead of guessing the highest-priority one.
	static ReplacementAnimFileInfo* EvaluateWinningInfo(
		const std::vector<ReplacementAnimFileInfo*>& a_candidates,
		RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen)
	{
		// A non-interruptible SubMod already active on this clip stays the winner
		// (the Update hook skips re-evaluation for it) — honor that here so the
		// pre-swap doesn't build the control from a different file on re-Activate.
		{
			SubMod* active = ValidatedActiveSubMod(a_clipGen);
			if (active && !active->IsInterruptible() && !active->IsDisabled()) {
				for (auto* info : a_candidates) {
					if (info && info->parentSubMod == active) return info;
				}
			}
		}

		for (auto* info : a_candidates) {
			if (!info || !info->parentSubMod) continue;
			if (info->parentSubMod->IsDisabled()) continue;
			if (!ClaimSkeletonAllowed(a_clipGen, info)) continue;
			auto* cs = info->parentSubMod->GetConditionSet();
			if (!cs || cs->IsEmpty()) return info;
			if (!a_refr) continue;
			try {
				if (info->parentSubMod->EvaluateConditions(a_refr, a_clipGen)) return info;
			} catch (...) { continue; }
		}
		return nullptr;
	}

	struct StandaloneSpecialIdleMatch
	{
		ReplacementAnimFileInfo* info = nullptr;
		std::string suffix;
		RE::hkaAnimation* animation = nullptr;
		std::vector<int16_t> donorTrackToBone;
		bool donorIdentity = false;
	};

	static void SetStandaloneSpecialIdleRejection(std::string* a_reason, std::string a_value)
	{
		if (a_reason) *a_reason = std::move(a_value);
	}

	// SetupSpecialIdle has no hkbClipGenerator yet, so resolve its IDLE filename
	// through the same leaf tables used by clip replacement. Only explicitly
	// opted-in track filters participate; clip-dependent conditions naturally
	// fail when evaluated with a null clip.
	static bool FindStandaloneSpecialIdleMatch(
		RE::Actor* a_actor, RE::TESIdleForm* a_idle, StandaloneSpecialIdleMatch& a_out,
		std::string* a_rejectionReason)
	{
		if (!a_actor) {
			SetStandaloneSpecialIdleRejection(a_rejectionReason, "actor is null");
			return false;
		}
		if (!a_idle) {
			SetStandaloneSpecialIdleRejection(a_rejectionReason, "idle form is null");
			return false;
		}
		const char* fileName = a_idle->animFileName.c_str();
		if (!fileName || !fileName[0]) {
			SetStandaloneSpecialIdleRejection(a_rejectionReason, "idle form has no animation filename");
			return false;
		}

		const auto rawSuffix = ExtractAnimSuffix(fileName);
		if (rawSuffix.empty()) {
			SetStandaloneSpecialIdleRejection(a_rejectionReason,
				std::format("could not extract a replacement suffix from '{}'", fileName));
			return false;
		}
		const std::string leaf(GetSuffixLeaf(rawSuffix));

		std::vector<std::pair<std::string, ReplacementAnimFileInfo*>> candidates;
		{
			std::shared_lock lock(s_nameLookupMutex);
			auto leafIt = s_leafToFullSuffixes.find(leaf);
			if (leafIt == s_leafToFullSuffixes.end()) {
				SetStandaloneSpecialIdleRejection(a_rejectionReason,
					std::format("no replacement candidate has leaf '{}'", leaf));
				return false;
			}
			for (const auto& suffix : leafIt->second) {
				auto infoIt = s_suffixToInfos.find(suffix);
				if (infoIt == s_suffixToInfos.end()) continue;
				for (auto* info : infoIt->second) {
					candidates.emplace_back(suffix, info);
				}
			}
		}

		std::ranges::stable_sort(candidates, [](const auto& a, const auto& b) {
			const int ap = a.second && a.second->parentSubMod ? a.second->parentSubMod->GetPriority() : INT_MIN;
			const int bp = b.second && b.second->parentSubMod ? b.second->parentSubMod->GetPriority() : INT_MIN;
			return ap > bp;
		});

		auto* cache = AnimationCache::GetSingleton();
		if (!cache->GetGameAnimVtable()) {
			SetStandaloneSpecialIdleRejection(a_rejectionReason, "game animation vtable is unavailable");
			return false;
		}
		std::string lastCandidateRejection = "no candidate was eligible";
		for (auto& [suffix, info] : candidates) {
			if (!info || !info->parentSubMod) {
				lastCandidateRejection = "replacement candidate has no owning submod";
				continue;
			}
			auto* subMod = info->parentSubMod;
			auto& filter = subMod->trackFilter;
			if (subMod->IsDisabled()) {
				lastCandidateRejection = std::format("submod '{}' is disabled", subMod->GetName());
				continue;
			}
			if (!filter.enabled) {
				lastCandidateRejection = std::format("submod '{}' has no track filter", subMod->GetName());
				continue;
			}
			if (!filter.triggerOnlySpecialIdle) {
				lastCandidateRejection = std::format(
					"submod '{}' has filter-only special-idle playback disabled", subMod->GetName());
				continue;
			}
			// Preserve ordinary path semantics: a foreign folder is eligible only
			// when this submod explicitly claims filenames through Leaf Matching.
			if (suffix != rawSuffix && !subMod->GetLeafMatching()) {
				lastCandidateRejection = std::format(
					"submod '{}' matched the leaf but Leaf Matching is disabled", subMod->GetName());
				continue;
			}

			auto* conditions = subMod->GetConditionSet();
			if (conditions && !conditions->IsEmpty()) {
				try {
					if (!subMod->EvaluateConditions(a_actor, nullptr)) {
						lastCandidateRejection = std::format(
							"submod '{}' conditions evaluated false", subMod->GetName());
						continue;
					}
				} catch (...) {
					lastCandidateRejection = std::format(
						"submod '{}' condition evaluation threw an exception", subMod->GetName());
					continue;
				}
			}

			auto* animation = cache->GetCachedAnimation(suffix, subMod);
			if (!animation || reinterpret_cast<uintptr_t>(animation) < 0x10000 ||
				IsBadReadPtr(animation, sizeof(RE::hkaAnimation))) {
				lastCandidateRejection = std::format(
					"submod '{}' has no valid cached donor animation", subMod->GetName());
				continue;
			}
			std::vector<int16_t> donorMap;
			bool donorIdentity = false;
			if (!cache->GetDonorTrackMap(suffix, subMod, donorMap, donorIdentity) ||
				(donorMap.empty() && !donorIdentity)) {
				logger::warn(
					"[OAR-TrackFilter-Standalone] '{}' matched '{}' but its donor binding has no track map; using vanilla SetupSpecialIdle",
					subMod->GetName(), fileName);
				lastCandidateRejection = std::format(
					"submod '{}' donor binding has no track map", subMod->GetName());
				continue;
			}

			a_out.info = info;
			a_out.suffix = suffix;
			a_out.animation = animation;
			a_out.donorTrackToBone = std::move(donorMap);
			a_out.donorIdentity = donorIdentity;
			return true;
		}
		SetStandaloneSpecialIdleRejection(a_rejectionReason, std::move(lastCandidateRejection));
		return false;
	}

	static bool StartStandaloneSpecialIdle(
		RE::Actor* a_actor, RE::TESIdleForm* a_idle, std::string* a_rejectionReason)
	{
		StandaloneSpecialIdleMatch match;
		if (!FindStandaloneSpecialIdleMatch(a_actor, a_idle, match, a_rejectionReason)) return false;

		auto* subMod = match.info->parentSubMod;
		auto* filter = &subMod->trackFilter;
		const float nowSec = s_tfNowSec.load(std::memory_order_relaxed);
		bool isNew = false;
		{
			std::unique_lock lock(s_trackFilterMutex);
			auto* state = FindTrackFilterState(a_actor, filter);
			isNew = state == nullptr;
			if (isNew) {
				state = &s_charTrackFilterMap[a_actor].emplace_back();
				state->filter = filter;
				s_trackFilterActiveCount.fetch_add(1, std::memory_order_relaxed);
			}

			state->replacement = match.animation;
			state->sourceAnimation = nullptr;
			state->parentSubMod = subMod;
			state->sourceClip = nullptr;
			state->sourceClips.clear();
			state->suffix = match.suffix;
			state->standaloneSpecialIdle = true;
			state->standaloneStartSec = nowSec;
			state->lastStandaloneSampleFrame = UINT64_MAX;
			state->donorTrackToBone = std::move(match.donorTrackToBone);
			state->donorMapIdentity = match.donorIdentity;
			state->donorMapQueried = true;
			state->cachedRepByName.clear();
			state->cachedBaseByName.clear();
			state->cacheValid = false;
			state->cameraDonorFrameZeroTracks.clear();
			state->invalidCameraReferenceTracks.clear();
			state->frozenByName.clear();
			state->lastSourceTimeSec = nowSec;
			state->lastSampleSec = 0.0f;
			state->lastSampledLocalTime = -1.0f;
			state->lastAdvanceSec = nowSec;
			state->selfAdvanceStartSec = -1.0f;
			state->selfAdvanceBaseTime = 0.0f;
			state->earlyBlendOutArmed = false;
			state->oneShotDone = false;
			state->dormant = false;
			state->sampleStarved = false;
			state->onEndFired = false;
			state->blendingOut = false;
			state->deactivationDelayActive = false;
			state->blendElapsed = 0.0f;
			state->blendDuration = filter->blendInTime;
			state->blendAlpha = filter->blendInTime <= 0.0f ? 1.0f : 0.0f;
		}

		QueueCustomEvents(a_actor, subMod->eventsOnStart, "onStart/trackFilter-specialIdle");
		logger::info(
			"[OAR-TrackFilter-Standalone] Started '{}' for actor {:X}: idle='{}' donor='{}' duration={:.3f}s{}",
			subMod->GetName(), a_actor->GetFormID(), a_idle->animFileName.c_str(), match.suffix,
			match.animation->duration, isNew ? "" : " (restarted)");
		return true;
	}

	// Pre-_Activate binding-set scrub. A FRESH clip generator has no animation
	// control at Activate-hook entry (the control is built inside _Activate), so
	// every GetAnimationSlot()-based restore is blind to a stale clone parked in
	// the shared per-character binding by a previous play. _Activate then builds
	// the play from the CLONE: the engine culls the clip's triggers against the
	// bound animation's duration, so every native annotation past the clone's
	// end is gone from the play before our first Update can restore the slot
	// (MP7A2 2026-08-19: empty reload evaluated correctly at t=0.000 and STILL
	// dropped 05_Bolt/06_Shoulder — 9-of-12 trigger array, cropped at the
	// clone's 2.292s). This path reaches the binding WITHOUT the control:
	// context -> character -> animationBindingSet[clip->animationBindingIndex],
	// all valid pre-activation (the Activate-time suffix resolvers already use
	// animationBindingIndex the same way).
	//
	// Self-validating by construction: the only write happens when the pointer
	// read from the binding is one of OUR live clone pointers (cache lookup) and
	// its recorded original passes the pointer/vtable checks — a wrong walk or a
	// wrong layout assumption reads a value that matches nothing and does
	// nothing. Layout assumption (hkbAnimationBindingSet: bindings hkArray at
	// +0x10 data / +0x18 size; hkaAnimationBinding: animation at +0x18, same as
	// the control-side slot) is additionally bounded by the size sanity check.
	//
	// The overlap cost is accepted: if another generator is mid-play on the
	// same binding with this clone, the scrub yanks it for one frame and that
	// generator's next Update re-asserts the swap (standard per-frame re-assert)
	// — brief, rare (reload-cancel spam), and strictly better than a play with
	// culled triggers.
	static void ScrubStaleCloneFromBindingSet(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context,
		int32_t a_playerGraphIndex)
	{
		// Walk-failure diagnostics (2026-08-19: the scrub silently never fired
		// in the field). Benign outcomes (walk succeeded, nothing of ours
		// bound) stay silent; every abnormal bail logs its stage, capped.
		static std::atomic<int> s_scrubBailLog{ 0 };
		auto bail = [&](const char* a_stage) {
			if (s_scrubBailLog.fetch_add(1, std::memory_order_relaxed) < 20) {
				logger::info("[OAR-Scrub] bail: {} (clipGen={:X}, playerGraph={})",
					a_stage, reinterpret_cast<uintptr_t>(a_this), a_playerGraphIndex);
			}
		};

		if (!a_this || !a_context) return;
		if (!s_gameFullyLoaded.load()) return;

		RE::BSTSmartPointer<RE::BSAnimationGraphManager> playerManager;
		RE::hkbCharacter* character = nullptr;
		if (a_playerGraphIndex >= 0) {
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->GetAnimationGraphManagerImpl(playerManager) || !playerManager) {
				return bail("player graph manager unavailable");
			}
			const auto graphIndex = static_cast<uint32_t>(a_playerGraphIndex);
			if (graphIndex >= playerManager->graph.size() || !playerManager->graph[graphIndex]) {
				return bail("resolved player graph unavailable");
			}
			// hkbContext::character is a static dummy in this runtime. The actual
			// binding set lives on the owning BShkbAnimationGraph character.
			character = &playerManager->graph[graphIndex]->character;
		} else {
			character = a_context->character;
		}
		if (!character || IsBadReadPtr(character, sizeof(RE::hkbCharacter))) return bail("character null/unreadable");

		auto* bindingSet = character->animationBindingSet._ptr;
		if (!bindingSet || IsBadReadPtr(bindingSet, 0x20)) return bail("bindingSet null/unreadable");

		const auto* setBytes = reinterpret_cast<const uint8_t*>(bindingSet);
		auto* const* bindings = *reinterpret_cast<uintptr_t* const* const*>(setBytes + 0x10);
		const int32_t bindingCount = *reinterpret_cast<const int32_t*>(setBytes + 0x18);
		const int16_t bindIdx = a_this->animationBindingIndex;
		if (!bindings || bindingCount <= 0 || bindingCount > 4096) return bail("bindings array invalid");
		if (bindIdx < 0 || bindIdx >= bindingCount) {
			static std::atomic<int> s_scrubIdxLog{ 0 };
			if (s_scrubIdxLog.fetch_add(1, std::memory_order_relaxed) < 20) {
				logger::info("[OAR-Scrub] bail: bindIdx {} out of range [0,{}) (clipGen={:X})",
					bindIdx, bindingCount, reinterpret_cast<uintptr_t>(a_this));
			}
			return;
		}
		if (IsBadReadPtr(bindings + bindIdx, sizeof(uintptr_t))) return bail("binding entry unreadable");

		auto* binding = reinterpret_cast<uint8_t*>(bindings[bindIdx]);
		if (!binding || IsBadReadPtr(binding, 0x20)) return;

		auto** slotAddr = reinterpret_cast<RE::hkaAnimation**>(binding + 0x18);
		auto* bound = *slotAddr;
		if (!bound) return;

		auto* cache = AnimationCache::GetSingleton();
		if (!cache->IsOurReplacement(bound)) return;

		auto* orig = cache->GetOriginalFromReplacement(bound);
		if (!orig || IsBadReadPtr(orig, sizeof(uintptr_t)) ||
			!IsPlausibleGameAnimVtable(*reinterpret_cast<uintptr_t*>(orig))) {
			return;
		}

		*slotAddr = orig;
		static std::atomic<int> s_scrubLog{ 0 };
		if (s_scrubLog.fetch_add(1, std::memory_order_relaxed) < 30) {
			logger::info("[OAR] Activation: scrubbed stale clone from binding set (clipGen={:X}, bindIdx={}, playerGraph={})",
				reinterpret_cast<uintptr_t>(a_this), bindIdx, a_playerGraphIndex);
		}
	}

	void hkbClipGenerator_Activate(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context)
	{
		const int32_t playerGraphIndex = PlayerGraphIndexForClip(a_this, a_context);
		if (playerGraphIndex >= 0) {
			std::unique_lock lock(s_playerClipMutex);
			s_playerClipGraph[a_this] = static_cast<uint8_t>(playerGraphIndex);
		}

		// Clean the shared binding BEFORE anything reads it: _Activate builds
		// the control and the play's trigger window from whatever is bound
		// here, and the pre-swap below re-installs the current winner's clone
		// afterwards when conditions pass — so scrubbing first is correct in
		// both directions (see ScrubStaleCloneFromBindingSet).
		ScrubStaleCloneFromBindingSet(a_this, a_context, playerGraphIndex);

		// Detect the transition after scrubbing the currently activating binding,
		// then retire all clones so this play rebuilds from the weapon's fresh game
		// animation. The retired records retain their original reverse links for
		// any other shared binding encountered later.
		if (playerGraphIndex >= 0 && s_gameFullyLoaded.load()) {
			CheckAndInvalidateOnWeaponChange();
		}

		// PRE-SWAP: If we have a cached replacement for this clip, swap it in BEFORE
		// the original _Activate runs. This ensures the hkaDefaultAnimationControl
		// is built from our clone (which has NULLed annotationTracks), preventing
		// stale pointer crashes in computeMotion/clearAndDeallocate.
		RE::hkaAnimation* preSwapOriginal = nullptr;
		bool preSwapAttempted = false;
		bool preSwapSucceeded = false;
		// Condition-passing winner from the pre-swap evaluation (null when the
		// pre-swap fell back to the highest-priority file with no winner).
		// Used to NULL the clip's triggers immediately at activation: the first
		// _Update processes the [0, dt] trigger window BEFORE the Update hook's
		// post-code runs, so without this the ORIGINAL animation's t~0
		// annotations (WeaponFire on fire clips) fire once on every activation
		// even though a replacement is about to install.
		ReplacementAnimFileInfo* preSwapWinner = nullptr;
		// The global "Enabled" toggle gates the pre-swap too — while disabled the
		// control must be built from the vanilla animation. (Anim-log entries are
		// registered further down regardless: the log is a monitoring tool.)
		if (s_gameFullyLoaded.load() && s_hasActiveReplacements.load() && a_this && s_lookupBuilt &&
			Settings::GetSingleton()->bEnabled) {
			const char* clipName = a_this->animationName.data();
			if (clipName && reinterpret_cast<uintptr_t>(clipName) > 0x10000 && clipName[0] != '\0') {
				// Direct path matching: prefer the exact suffix of the clip's
				// poll-resolved real path (leaf-validated). Falls back to the
				// authored-name/leaf-matching derivation when unavailable —
				// including whenever the toggle is off.
				std::string activeSuffix = DirectSuffixFromCachedPath(a_this);
				if (activeSuffix.empty()) {
					// Direct path matching: for clips in the PLAYER's graphs the
					// per-frame poll resolves the real on-disk path within a few
					// frames. Pre-swapping on the authored/leaf-derived guess can
					// pick the WRONG submod (a bare-leaf registration hijacks
					// folder-scoped animations — the '1911 Idle Empty' bug), so
					// skip the pre-swap; the Update hook installs the correct
					// replacement once the poll has resolved the path (see the
					// direct-path defer gate there). Everything else (toggle off,
					// non-player actors, unattributable clips) keeps the
					// leaf-fallback pre-swap.
					const bool deferForDirectPath =
						Settings::GetSingleton()->bDirectPathMatching &&
						PlayerGraphIndexForClip(a_this, a_context) >= 0;
					if (!deferForDirectPath) {
						activeSuffix = ResolveOrLeafFallback(ExtractAnimSuffix(std::string(clipName)));
					}
				}
				if (!activeSuffix.empty()) {
					// Check if this suffix has a registered replacement at all
					bool hasRegistered = false;
					{
						std::shared_lock rlock(s_nameLookupMutex);
						if (activeSuffix.size() > 6 && activeSuffix.substr(0, 6) == "multi:") {
							std::string leaf = activeSuffix.substr(6);
							hasRegistered = s_leafToFullSuffixes.find(leaf) != s_leafToFullSuffixes.end();
						} else {
							hasRegistered = s_suffixToInfos.find(activeSuffix) != s_suffixToInfos.end();
						}
					}

					if (hasRegistered) {
						auto** animSlotPre = a_this->GetAnimationSlot();
						if (animSlotPre && *animSlotPre) {
							auto* cachePre = AnimationCache::GetSingleton();
							RE::hkaAnimation* replacement = nullptr;

							// Evaluate conditions NOW (same loop as the Update hook) so the
							// animation control is built from the winning SubMod's file.
							//
							// CRITICAL: only pre-swap when a condition-passing winner exists.
							// The old "fallback pre-swap any highest-priority file" path built
							// the control from a clone with emptied annotationTracks, then
							// restored the original into the slot when Update decided NOT to
							// replace. Result: the clip played the original visuals, but its
							// control had no annotation-derived triggers — end sounds and
							// events (reloadEnd, final Foley, etc.) silently never fired.
							// Field case (AE, SCAR Reload Variants): empty-mag reloads failed
							// the submod's `NOT CurrentMagazineAmmo==0` condition, so OAR
							// never managed annotations, yet Activate had already stripped
							// the native ones. Skip the pre-swap entirely when there is no
							// winner; _Activate then builds the control from the real
							// original and native annotations work. If conditions flip true
							// a few frames later, the Update hook installs the replacement
							// (same path as a deferred direct-path resolve).
							RE::TESObjectREFR* refrPre = GetRefrFromContext(a_context);
							if (!refrPre) refrPre = RE::PlayerCharacter::GetSingleton();

							if (activeSuffix.size() > 6 && activeSuffix.substr(0, 6) == "multi:") {
								std::string leafName = activeSuffix.substr(6);
								std::shared_lock rlock(s_nameLookupMutex);
								auto leafIt = s_leafToFullSuffixes.find(leafName);
								if (leafIt != s_leafToFullSuffixes.end()) {
									// Same selection rules as the Update hook's multi
									// block: (1) Leaf Matching claims first — flagged
									// submods only, the clip's own registered suffix
									// preferred, ELSE the override-list order; (2) the
									// exact suffix with normal winner rules; (3) the
									// legacy leaf-fallback loop ONLY when the clip's
									// real path is unknown (a known path is settled
									// identity — foreign paths' unflagged candidates
									// must not pre-swap onto it).
									std::string exactPre = RealSuffixForClip(a_this, leafName);

									auto ovIt = s_leafOverrideSuffixes.find(leafName);
									if (ovIt != s_leafOverrideSuffixes.end()) {
										auto tryFlagged = [&](const std::string& a_ovSuffix) -> bool {
											auto candIt = s_suffixToInfos.find(a_ovSuffix);
											if (candIt == s_suffixToInfos.end()) return false;
											for (auto* info : candIt->second) {
												if (!info || !info->parentSubMod) continue;
												if (!info->parentSubMod->GetLeafMatching()) continue;
												if (info->parentSubMod->IsDisabled()) continue;
												if (!ClaimSkeletonAllowed(a_this, info)) continue;
												auto* cs = info->parentSubMod->GetConditionSet();
												bool pass = (!cs || cs->IsEmpty());
												if (!pass && refrPre) {
													try {
														pass = info->parentSubMod->EvaluateConditions(refrPre, a_this);
													} catch (...) { pass = false; }
												}
												if (!pass) continue;
												replacement = cachePre->GetOrBuildRuntimeAnim(
													a_ovSuffix, *animSlotPre, info->parentSubMod);
												if (replacement) {
													preSwapWinner = info;
													return true;
												}
											}
											return false;
										};
										bool claimed = false;
										if (!exactPre.empty() &&
											std::ranges::find(ovIt->second, exactPre) != ovIt->second.end()) {
											claimed = tryFlagged(exactPre);
										}
										if (!claimed) {
											for (const auto& ovSuffix : ovIt->second) {
												if (ovSuffix == exactPre) continue;
												if (tryFlagged(ovSuffix)) break;
											}
										}
									}

									if (!replacement && !exactPre.empty()) {
										auto candIt = s_suffixToInfos.find(exactPre);
										if (candIt != s_suffixToInfos.end()) {
											if (auto* winner = EvaluateWinningInfo(candIt->second, refrPre, a_this)) {
												replacement = cachePre->GetOrBuildRuntimeAnim(
													exactPre, *animSlotPre, winner->parentSubMod);
												if (replacement) preSwapWinner = winner;
											}
										}
									}

									// Legacy leaf-bridging (direct path matching OFF)
									// keeps its fallback loop even with a known path.
									if (!replacement && (exactPre.empty() ||
											!Settings::GetSingleton()->bDirectPathMatching))
									for (const auto& fullSuffix : leafIt->second) {
										auto candIt = s_suffixToInfos.find(fullSuffix);
										if (candIt == s_suffixToInfos.end()) continue;
										if (auto* winner = EvaluateWinningInfo(candIt->second, refrPre, a_this)) {
											replacement = cachePre->GetOrBuildRuntimeAnim(
												fullSuffix, *animSlotPre, winner->parentSubMod);
											if (replacement) {
												preSwapWinner = winner;
												break;
											}
										}
									}
								}
							} else {
								ReplacementAnimFileInfo* winner = nullptr;
								{
									std::shared_lock rlock(s_nameLookupMutex);
									auto candIt = s_suffixToInfos.find(activeSuffix);
									if (candIt != s_suffixToInfos.end()) {
										winner = EvaluateWinningInfo(candIt->second, refrPre, a_this);
									}
								}
								if (winner) {
									replacement = cachePre->GetOrBuildRuntimeAnim(activeSuffix, *animSlotPre,
										winner->parentSubMod);
									if (replacement) preSwapWinner = winner;
								}
							}

							// No winner this activation, but the slot may still hold OUR
							// clone installed by a PREVIOUS play of this clip: the
							// completion path restores triggers, not the slot, and the
							// Update hook's restore comes one tick too late here —
							// _Activate builds the clip's animation control from whatever
							// is in the slot, and a clone has emptied annotationTracks.
							// Field case (HK416 empty reload right after a tactical
							// reload): control built from the stale 2.2s clone, conditions
							// then failed (mag empty), Update restored the original
							// visuals — but the play had NO native annotations, so
							// reloadComplete/reloadEnd never fired and the magazine never
							// refilled. Restore the original NOW so the control is built
							// from the real animation. A retired clone with no recorded
							// original is left alone (orphan recovery handles it).
							if (!replacement && cachePre->IsOurReplacement(*animSlotPre)) {
								auto* staleOrig = cachePre->GetOriginalFromReplacement(*animSlotPre);
								// Validate before writing into the slot: the recorded
								// original can be freed (weapon switch since that play),
								// and the multi-clone cache keeps old clones alive with
								// their stale originals recorded.
								if (staleOrig && !IsBadReadPtr(staleOrig, sizeof(uintptr_t)) &&
									IsPlausibleGameAnimVtable(*reinterpret_cast<uintptr_t*>(staleOrig))) {
									*animSlotPre = staleOrig;
									logger::info("[OAR] Activation: restored original over stale replacement for '{}' (no winner)",
										activeSuffix);
								} else {
									logger::warn("[OAR] Activation: stale replacement in slot for '{}' has no valid recorded original — leaving as-is",
										activeSuffix);
								}
							}

							// Mark attempted only when we had a winner to install. A
							// no-winner skip must NOT put the clip in the bypass set —
							// Update still needs to be able to swap if conditions flip.
							if (replacement) {
								preSwapAttempted = true;
								auto repVtbl = *reinterpret_cast<uintptr_t*>(replacement);
								if (IsInGameModule(repVtbl)) {
									preSwapOriginal = *animSlotPre;
									*animSlotPre = replacement;
									preSwapSucceeded = true;
								}
							}
						}
					}
				}
			}
		}

		// Catch-all stale-clone guard, independent of suffix resolution: when
		// this activation is NOT pre-swapping a replacement, the animation
		// control about to be built by _Activate must be built from the REAL
		// original. The targeted restore above only runs when the Activate-time
		// suffix resolved AND was registered — a pre-swap deferred for
		// direct-path resolution (fresh player clips) or an unregistered guess
		// skipped it entirely, so a clone left by the PREVIOUS play stayed in
		// the slot through _Activate. The control then carried the CLONE's
		// duration: the state's relative-to-end exit trigger fired at the
		// clone's end, cutting off every native annotation past it on the
		// un-replaced play (MP7 empty reload dropping 05_Bolt/06_Shoulder,
		// 2026-08-18 — same family as the HK416 case the targeted restore was
		// built for).
		if (!preSwapSucceeded && s_gameFullyLoaded.load() && a_this) {
			if (auto** slotPre2 = a_this->GetAnimationSlot(); slotPre2 && *slotPre2) {
				auto* cacheGuard = AnimationCache::GetSingleton();
				if (cacheGuard->IsOurReplacement(*slotPre2)) {
					auto* staleOrig = cacheGuard->GetOriginalFromReplacement(*slotPre2);
					if (staleOrig && !IsBadReadPtr(staleOrig, sizeof(uintptr_t)) &&
						IsPlausibleGameAnimVtable(*reinterpret_cast<uintptr_t*>(staleOrig))) {
						*slotPre2 = staleOrig;
						static std::atomic<int> s_catchAllRestoreLog{ 0 };
						if (s_catchAllRestoreLog.fetch_add(1, std::memory_order_relaxed) < 30) {
							logger::info("[OAR] Activation: catch-all restored original over stale clone (clipGen={:X}, no pre-swap this activation)",
								reinterpret_cast<uintptr_t>(a_this));
						}
					}
				}
			}
		}

		// If pre-swap was attempted but failed, add to bypass set so Update won't
		// try to swap a clone into a slot whose animation control was built from
		// a different animation — that mismatch causes crashes.
		if (preSwapAttempted && !preSwapSucceeded && a_this) {
			std::unique_lock lock(s_bypassMutex);
			s_bypassSet.insert(a_this);
		} else if (preSwapSucceeded && a_this) {
			// Pre-swap worked, ensure this clip is NOT in bypass
			std::unique_lock lock(s_bypassMutex);
			s_bypassSet.erase(a_this);
		}

		Hooks::ClipGeneratorHooks::_Activate(a_this, a_context);

		// If we pre-swapped, restore the original in the slot so the Update hook
		// can properly evaluate conditions and decide whether to keep the replacement.
		// The animation control has already been built with empty annotationTracks.
		RE::hkaAnimation* preSwapReplacementAnim = nullptr;
		if (preSwapOriginal) {
			auto** animSlotPost = a_this->GetAnimationSlot();
			if (animSlotPost) {
				preSwapReplacementAnim = *animSlotPost;
				*animSlotPost = preSwapOriginal;
			}
		}

		// A condition-passing winner means the Update hook WILL install this
		// replacement — NULL the triggers now so the very first _Update (which
		// runs before our post-code can do it) doesn't natively fire the
		// ORIGINAL animation's t~0 annotations (WeaponFire on fire clips).
		// Deliberately NOT done for the no-winner fallback pre-swap: there the
		// Update hook may decide against replacing, and NULLing would eat the
		// original's t=0 events for that first frame (missed real WeaponFire).
		//
		// This is the FIRST trigger-array build of the play for pre-swapped
		// clips (backup.nulled blocks later rebuilds), so End Clip If Shorter's
		// re-timing must be decided here, not just in the Update hook.
		if (preSwapSucceeded && preSwapWinner && preSwapWinner->parentSubMod &&
			preSwapWinner->parentSubMod->GetReplaceAnnotations() &&
			!preSwapWinner->parentSubMod->GetPlayOnceFullBody()) {
			const float endClipDurAct = EndClipIfShorterDuration(
				preSwapWinner->parentSubMod, a_this, preSwapReplacementAnim, preSwapOriginal);
			InstallReplacementTriggers(a_this, "", endClipDurAct);
			static int s_actNullLog = 0;
			if (s_actNullLog < 20) {
				logger::info("[OAR-Triggers] Activation pre-NULL for clipGen={:X} (winner '{}')",
					reinterpret_cast<uintptr_t>(a_this),
					preSwapWinner->parentSubMod->GetName());
				s_actNullLog++;
			}
		}

		if (!s_gameFullyLoaded.load() || !s_hasActiveReplacements.load() || !a_this) {
			return;
		}

		if (!s_lookupBuilt) BuildNameLookup();
		if (!s_idleAnimReverseBuilt.load()) BuildIdleAnimReverseMap();

		{
			static std::atomic<int> s_refreshCounter{ 0 };
			int count = s_refreshCounter.fetch_add(1);
			if (!s_weaponAnimFolderValid.load() || (count % 2000 == 0)) {
				RefreshWeaponAnimFolder();
			}
		}

		auto* cache = AnimationCache::GetSingleton();

		auto* currentAnim = a_this->GetAnimation();
		if (currentAnim && cache->GetGameAnimVtable() == 0) {
			auto vtbl = *reinterpret_cast<uintptr_t*>(currentAnim);
			cache->SetVtableFromGame(vtbl);
			logger::info("[OAR] Captured game hkaAnimation vtable: {:X}", vtbl);
		}

		// Resolve Havok vtables from REL::ID for building replacement trigger arrays
		ResolveHavokVtables();

		// Diagnostic: log m_animationPath per character (once per unique character pointer)
		if (a_context && a_context->character) {
			static std::shared_mutex s_loggedCharsMutex;
			static std::unordered_set<RE::hkbCharacter*> s_loggedChars;

			auto* character = a_context->character;
			bool shouldLog = false;
			{
				std::shared_lock slock(s_loggedCharsMutex);
				shouldLog = (s_loggedChars.find(character) == s_loggedChars.end());
			}
			if (shouldLog) {
				std::unique_lock ulock(s_loggedCharsMutex);
				if (s_loggedChars.insert(character).second) {
					const char* charName = character->name.data();
					std::string animPathStr = "(unavailable)";

					auto* projData = character->projectData._ptr;
					if (projData && !IsBadReadPtr(projData, 0x30)) {
						auto* projStrData = projData->stringData._ptr;
						if (projStrData && !IsBadReadPtr(projStrData, 0x80)) {
							const char* rawPath = projStrData->animationPath.data();
							if (rawPath && reinterpret_cast<uintptr_t>(rawPath) > 0x10000 &&
								!IsBadReadPtr(rawPath, 1) && rawPath[0] != '\0')
							{
								animPathStr = rawPath;
							}
						}
					}

					logger::info("[OAR-ProjectData] Character='{}' ptr={:X} animationPath='{}'",
						charName ? charName : "(null)",
						reinterpret_cast<uintptr_t>(character),
						animPathStr);
				}
			}
		}

		// Cache the clip suffix (animationName is valid at Activate time but may be cleared later)
		auto suffix = GetClipSuffixFromContext(a_this, a_context);
		bool suffixChanged = false;
		if (!suffix.empty()) {
			// Log first few cached suffixes for diagnostics
			static int s_cacheLogCount = 0;
			if (s_cacheLogCount < 30) {
				std::shared_lock rlock(s_nameLookupMutex);
				bool hasMatch = s_suffixToInfos.find(suffix) != s_suffixToInfos.end();
				rlock.unlock();
				logger::info("[OAR-Activate] Cached suffix='{}' match={}", suffix, hasMatch);
				s_cacheLogCount++;
			}
			{
				std::unique_lock lock(s_clipSuffixMutex);
				auto it = s_clipSuffixCache.find(a_this);
				if (it == s_clipSuffixCache.end() || it->second != suffix) {
					suffixChanged = true;
					s_clipSuffixCache[a_this] = suffix;
				}
			}
			// If the suffix changed for this clipGen pointer (engine reused the slot for a
			// different logical clip), the cached "original" is now stale — clear it so the
			// new original gets captured below. The cached real path may be stale too —
			// but only drop it when its leaf doesn't match the NEW suffix, because
			// GetClipSuffixFromContext above may have just cached a fresh, correct
			// resolution for this activation (Source S).
			if (suffixChanged) {
				if (DisplayPathForEntry(a_this, suffix).empty()) {
					{
						std::unique_lock plock(s_clipRealPathMutex);
						s_clipRealPathCache.erase(a_this);
					}
					{
						std::unique_lock slock(s_clipRealPathStateMutex);
						s_clipRealPathAuthoritative.erase(a_this);
						s_clipRealPathAttempts.erase(a_this);
					}
				}
				std::unique_lock olock(s_originalAnimMutex);
				s_originalAnimMap.erase(a_this);
				std::unique_lock alock(s_annotStateMutex);
				s_annotStateMap.erase(a_this);
				static int s_resetLog = 0;
				if (s_resetLog < 30) {
					logger::info("[OAR-Activate] ClipGen {} reused for new suffix '{}' — reset original/annot state",
						reinterpret_cast<uintptr_t>(a_this), suffix);
					s_resetLog++;
				}
			}
		} else {
			// Log failure to read animation name
			static int s_failLogCount = 0;
			if (s_failLogCount < 10) {
				const char* rawName = a_this->animationName.data();
				uintptr_t rawPtr = reinterpret_cast<uintptr_t>(RE::GetHkStringRawPtr(a_this->animationName));
				logger::warn("[OAR-Activate] Failed to get suffix: rawPtr={:X}, bindIdx={}",
					rawPtr, static_cast<int>(a_this->animationBindingIndex));
				s_failLogCount++;
			}
		}

		// Record player-graph membership straight from the clip's behavior
		// graph. The poll can't see this clip until the graph stabilizes (it
		// skips graphs mid-rebuild), but log attribution and perspective
		// classification need the membership NOW — argument evaluation order
		// in the AddEntry calls below is unspecified, so don't rely on
		// ResolveLogRefr's backfill happening before ClassifyClipPerspective.
		if (const auto playerGi = PlayerGraphIndexForClip(a_this, a_context); playerGi >= 0) {
			std::unique_lock lock(s_playerClipMutex);
			s_playerClipGraph[a_this] = static_cast<uint8_t>(playerGi);
		}

		// Log this activation to the Animation Log for the UI
		if (AnimationLog::GetSingleton()->IsEnabled()) {
			std::string suffixCopy;
			{
				std::shared_lock rlock(s_clipSuffixMutex);
				auto sit = s_clipSuffixCache.find(a_this);
				if (sit != s_clipSuffixCache.end()) suffixCopy = sit->second;
			}
			if (!suffixCopy.empty()) {
				// Display path priority (mirrors GunMover's ResolveClipDisplayPath):
				//  1. Subgraph swap-array resolution (cached by Source S above)
				//  2. The clip's authored animation path (animationName — valid at
				//     Activate time; this is the full authored path, e.g.
				//     "Actors\Character\Animations\default\neutral\eyeblinkfull.hkx")
				// Leaf-validated: a recycled clip address may still carry the
				// previous animation's cached path (see DisplayPathForEntry).
				std::string displayPath = DisplayPathForEntry(a_this, suffixCopy);
				bool authoritative = false;
				{
					std::shared_lock slock(s_clipRealPathStateMutex);
					authoritative = s_clipRealPathAuthoritative.contains(a_this);
				}
				// A stale (mismatching) cached path also invalidates the
				// authoritative flag — it belongs to the previous animation.
				if (displayPath.empty()) authoritative = false;
				if (displayPath.empty() || !authoritative) {
					const char* authored = a_this->animationName.data();
					if (authored && reinterpret_cast<uintptr_t>(authored) > 0x10000 &&
						!IsBadReadPtr(authored, 1) && authored[0] != '\0') {
						displayPath = authored;
						// Backfill the cache so the Active Replacements window
						// (populated in Update, after animationName may be cleared)
						// can show at least the authored path. NOT marked
						// authoritative — the Update hook keeps retrying the
						// subgraph walk and overwrites this on success.
						std::unique_lock plock(s_clipRealPathMutex);
						s_clipRealPathCache[a_this] = displayPath;
					}
				}
				if (authoritative) {
					AnimationLog::GetSingleton()->AddEntry(
						AnimationLog::EventType::kActivate,
						ResolveLogRefr(a_this, a_context), suffixCopy, "", "",
						displayPath, ClassifyClipPerspective(a_this, displayPath));
				} else {
					// Subgraph walk can't succeed at Activate time (the graph is
					// mid-transition; see the deferred-resolution comment at the
					// cache declarations). Hold this entry back — the Update hook
					// flushes it once the per-frame player-graph poll resolved
					// the real path (or the grace period passes).
					std::unique_lock slock(s_clipRealPathStateMutex);
					s_pendingActivateLog[a_this] = PendingActivateLog{
						suffixCopy, s_currentFrame.load(std::memory_order_relaxed)
					};
				}
			}
		}

		// Store the original animation pointer on activation.
		// If the slot currently holds our replacement (from a previous clip that was
		// never formally deactivated), recover the original from the cache.
		auto** animSlot = a_this->GetAnimationSlot();
		if (animSlot && *animSlot) {
			auto* cache = AnimationCache::GetSingleton();
			if (!cache->IsOurReplacement(*animSlot)) {
				std::unique_lock lock(s_originalAnimMutex);
				s_originalAnimMap.try_emplace(a_this, *animSlot);
			} else {
				// Slot has our replacement — recover the true original
				RE::hkaAnimation* recovered = cache->GetOriginalFromReplacement(*animSlot);
				if (recovered) {
					std::unique_lock lock(s_originalAnimMutex);
					s_originalAnimMap[a_this] = recovered;
				}
			}
		}

		// Cache original animation's annotation strings for suppression (Step 2).
		// Parse the original hkaAnimation's annotationTracks and store event text per actor.
		RE::TESObjectREFR* activateRefr = GetRefrFromContext(a_context);
		if (!activateRefr) activateRefr = RE::PlayerCharacter::GetSingleton();
		if (activateRefr && animSlot && *animSlot) {
			auto* origAnim = *animSlot;
			auto* origBytes = reinterpret_cast<uint8_t*>(origAnim);
			auto* annotTrackPtr = *reinterpret_cast<uint8_t**>(origBytes + 0x28);
			int32_t annotTrackCount = *reinterpret_cast<int32_t*>(origBytes + 0x30);

			if (annotTrackPtr && annotTrackCount > 0 && reinterpret_cast<uintptr_t>(annotTrackPtr) > 0x10000) {
				uint32_t actorID = activateRefr->GetFormID();
				std::unordered_set<std::string> origAnnots;

				constexpr size_t kAnnotTrackSize = 0x18;
				constexpr size_t kAnnotationSize = 0x10;

				for (int32_t t = 0; t < annotTrackCount; ++t) {
					auto* trackBase = annotTrackPtr + (t * kAnnotTrackSize);
					auto* annots = *reinterpret_cast<uint8_t**>(trackBase + 0x08);
					int32_t annotCount = *reinterpret_cast<int32_t*>(trackBase + 0x10);
					if (!annots || annotCount <= 0 || reinterpret_cast<uintptr_t>(annots) < 0x10000) continue;

					for (int32_t a = 0; a < annotCount; ++a) {
						auto* annBase = annots + (a * kAnnotationSize);
						auto* txtPtr = *reinterpret_cast<const char**>(annBase + 0x08);
						auto rawTxt = reinterpret_cast<uintptr_t>(txtPtr) & ~uintptr_t(1);
						auto* txt = reinterpret_cast<const char*>(rawTxt);
						if (txt && rawTxt > 0x10000 && txt[0] != '\0') {
							origAnnots.insert(std::string(txt));
						}
					}
				}

				if (!origAnnots.empty()) {
					std::unique_lock olock(s_origAnnotSetMutex);
					auto& existing = s_origAnnotByActor[actorID];
					existing.insert(origAnnots.begin(), origAnnots.end());

					static int s_origAnnotLog = 0;
					if (s_origAnnotLog < 10) {
						logger::info("[OAR-Annot] Cached {} original annotations for actor {:X}",
							origAnnots.size(), actorID);
						s_origAnnotLog++;
					}
				}
			}
		}

		// Ensure the suppression sink is registered
		RegisterSuppressionSink();

	}

	// SEH wrapper for _Update — catches crashes in computeMotion due to stale annotation data.
	// Must be in its own function because __try cannot coexist with C++ exception handling.
	static bool SafeCallOriginalUpdate(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context, float a_timestep)
	{
		__try {
			Hooks::ClipGeneratorHooks::_Update(a_this, a_context, a_timestep);
			return true;
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	// SEH wrapper for NotifyAnimationGraphImpl — the crash in this session occurred
	// here at line 2156 when HaBCR traversed stale animation data during event broadcast.
	static bool SafeNotifyAnimGraph(RE::TESObjectREFR* a_refr, RE::BSFixedString& a_evtName)
	{
		__try {
			a_refr->NotifyAnimationGraphImpl(a_evtName);
			return true;
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	// SEH wrapper for NotifyEventSinks — also traverses the behavior graph event system
	static void SafeNotifyEventSinks(RE::TESObjectREFR* a_refr, RE::BSFixedString& a_evtName)
	{
		__try {
			NotifyEventSinks(a_refr, a_evtName);
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			static int s_sinkFailLog = 0;
			if (s_sinkFailLog < 10) {
				s_sinkFailLog++;
			}
		}
	}

	// Deferred custom event queue — eventsOnStart / eventsOnEnd only. Fired AFTER
	// RunActorUpdatesOrig() so custom events never nest inside Havok update.
	// Replacement annotations are NOT deferred (that desynced equip/melee timing);
	// they fire synchronously with a reentrancy guard + SEH instead.
	struct DeferredEvent {
		RE::TESObjectREFR* refr{ nullptr };
		std::string eventName;
		std::string label;
	};
	static std::mutex s_deferredEventMutex;
	static std::vector<DeferredEvent> s_deferredEvents;

	// Queue events for deferred firing (safe to call from within Havok update hooks).
	static void QueueCustomEvents(RE::TESObjectREFR* a_refr, const std::vector<std::string>& a_events, const char* a_label)
	{
		if (!a_refr || a_events.empty()) return;
		std::lock_guard lock(s_deferredEventMutex);
		for (const auto& evt : a_events) {
			if (evt.empty()) continue;
			s_deferredEvents.push_back({ a_refr, evt, a_label });
		}
	}

	// Process all queued custom events (call from outside Havok update).
	static void FlushDeferredEvents()
	{
		std::vector<DeferredEvent> batch;
		{
			std::lock_guard lock(s_deferredEventMutex);
			if (s_deferredEvents.empty()) return;
			batch.swap(s_deferredEvents);
		}
		for (auto& de : batch) {
			if (!de.refr) continue;
			RE::BSFixedString evtName(de.eventName.c_str());
			SafeNotifyAnimGraph(de.refr, evtName);
			SafeNotifyEventSinks(de.refr, evtName);

			if (de.eventName == "ReloadEnd" || de.eventName == "reloadEnd") {
				auto* actor = de.refr->As<RE::Actor>();
				if (actor) {
					SetHavokBool(actor, kHavokVar_IsReloading, false);
				}
			}

			static int s_ceLog = 0;
			if (s_ceLog < 50) {
				logger::info("[OAR-CustomEvent] Fired '{}' ({}) on {:X}", de.eventName, de.label,
					de.refr->GetFormID());
				s_ceLog++;
			}
		}
	}

	// Fire any not-yet-fired annotations of an ending replacement play.
	//
	// Annotation firing is driven by Update crossing each annotation's time, but
	// field logs (AE, SCAR reload replacements) show the behavior graph can stop
	// advancing a clip's localTime ~0.5s before the animation's end (state exits /
	// transition blends park the outgoing clip at a frozen time) while the clip
	// stays alive for a while and is then torn down — either via Deactivate or via
	// OAR's own condition-fail restore. Everything between the freeze point and
	// the end (final foley sounds, initiateStart, reloadEnd) was silently lost.
	//
	// This helper fires the remaining annotations if tracking got within
	// kEndFlushWindowSec of the end: sounds immediately (BSAudioManager is safe on
	// this thread), graph/sink events via the deferred queue (never notify the
	// graph from inside its own update/teardown; the queue also handles the
	// reloadEnd IsReloading cleanup). A genuine early interrupt (reload cancelled
	// halfway) leaves prevLocalTime outside the window and flushes nothing.
	// The clip's annotation tracking state is erased afterwards so a later
	// re-application re-initializes cleanly.
	static void FlushPendingEndAnnotations(RE::hkbClipGenerator* a_clip, RE::TESObjectREFR* a_refr, const char* a_reason)
	{
		// Wide window on purpose: the observed freeze points sat 0.5-0.65s before
		// the end (prevT 2.86 / duration 3.50, prevT 3.05 / duration 3.567). The
		// cost of a too-wide window is firing tail foley on a late manual cancel,
		// which is cosmetically harmless; the cost of a too-narrow one is losing
		// the end sound/event on every graph-side early exit.
		constexpr float kEndFlushWindowSec = 1.0f;

		std::string flushSuffix;
		const void* flushOwner = nullptr;
		float flushPrevT = -1.f;
		int32_t flushLastIdx = -1;
		{
			std::shared_lock alock(s_annotStateMutex);
			auto ait = s_annotStateMap.find(a_clip);
			if (ait == s_annotStateMap.end()) return;
			flushSuffix = ait->second.activeSuffix;
			flushOwner = ait->second.activeOwner;
			flushPrevT = ait->second.prevLocalTime;
			flushLastIdx = ait->second.lastFiredIndex;
		}
		if (flushSuffix.empty() || flushPrevT < 0.f) return;

		// Duration of the animation actually in the slot (the replacement clone
		// while a replacement is active) — same +0x14 read as the completion path.
		float duration = 0.f;
		if (auto** slot = a_clip->GetAnimationSlot()) {
			auto* anim = *slot;
			if (anim && !IsBadReadPtr(anim, 0x18)) {
				duration = *reinterpret_cast<float*>(
					reinterpret_cast<uint8_t*>(anim) + 0x14);
			}
		}

		const auto* annotations = AnimationCache::GetSingleton()->GetAnnotations(flushSuffix, flushOwner);
		const int32_t total = annotations ? static_cast<int32_t>(annotations->size()) : 0;

		if (annotations && flushLastIdx + 1 < total && duration > 0.01f) {
			if (flushPrevT >= duration - kEndFlushWindowSec) {
				auto* flushRefr = a_refr;
				if (!flushRefr) flushRefr = RE::PlayerCharacter::GetSingleton();
				// Honor the winning submod's annotation suppression config.
				SubMod* flushSubMod = nullptr;
				{
					std::shared_lock smLock(s_activeSubModMutex);
					auto smIt = s_activeSubModMap.find(a_clip);
					if (smIt != s_activeSubModMap.end()) flushSubMod = smIt->second;
				}
				std::vector<std::string> flushEvents;
				for (int32_t i = flushLastIdx + 1; i < total; ++i) {
					const auto& ann = (*annotations)[i];
					if (flushSubMod && flushSubMod->IsAnnotationSuppressed(ann.text)) continue;

					static constexpr const char* kSoundPlayPrefix = "SoundPlay.";
					static constexpr size_t kSoundPlayLen = 10;
					if (ann.text.size() > kSoundPlayLen &&
						_strnicmp(ann.text.c_str(), kSoundPlayPrefix, kSoundPlayLen) == 0)
					{
						if (flushRefr) PlaySoundDirect(ann.text.c_str() + kSoundPlayLen, flushRefr);
					} else {
						flushEvents.push_back(ann.text);
					}
					static int s_endFlushLog = 0;
					if (s_endFlushLog < 100) {
						logger::info("[OAR-Annot] End-flush '{}' (clip '{}', {}, prevT={:.3f} duration={:.3f})",
							ann.text, flushSuffix, a_reason, flushPrevT, duration);
						s_endFlushLog++;
					}
				}
				if (flushRefr && !flushEvents.empty()) {
					QueueCustomEvents(flushRefr, flushEvents, "annot end-flush");
				}
			} else {
				// Pending annotations exist but tracking stopped too far from the
				// end — treated as a genuine interrupt. Logged so freeze points
				// that outgrow the window are visible in the field.
				static int s_endFlushSkipLog = 0;
				if (s_endFlushSkipLog < 30) {
					logger::info("[OAR-Annot] End-flush skipped for '{}' ({}, prevT={:.3f} duration={:.3f}, {} pending)",
						flushSuffix, a_reason, flushPrevT, duration, total - (flushLastIdx + 1));
					s_endFlushSkipLog++;
				}
			}
		}

		{
			std::unique_lock alock(s_annotStateMutex);
			s_annotStateMap.erase(a_clip);
		}
	}

	void hkbClipGenerator_Update(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context, float a_timestep)
	{
		// Call original Update first — variable bindings must process before any animation swap.
		if (!SafeCallOriginalUpdate(a_this, a_context, a_timestep)) {
			static int s_sehLog = 0;
			if (s_sehLog < 20) {
				auto** slot = a_this ? a_this->GetAnimationSlot() : nullptr;
				logger::error("[OAR-SEH] computeMotion crash caught! clipGen={:X} animSlot={:X} anim={:X}",
					reinterpret_cast<uintptr_t>(a_this),
					reinterpret_cast<uintptr_t>(slot),
					slot ? reinterpret_cast<uintptr_t>(*slot) : 0);
				s_sehLog++;
			}
			return;
		}

		if (!s_gameFullyLoaded.load() || !s_hasActiveReplacements.load() || !a_this || !s_lookupBuilt) {
			return;
		}

		// ===== Deferred kActivate anim-log flush =====
		// Path resolution itself happens in PollPlayerGraphClips() (per-frame,
		// outside graph update — GunMover's model). Here we only flush the
		// held-back kActivate entry once the poll resolved this clip's real
		// path, or after a short grace period (~10 frames) for clips that will
		// never resolve (creature graphs with no swap array). Attribution and
		// perspective come from player-graph membership, not the context.
		// This must run BEFORE the bypass early-out below: bypassed clips are
		// typically the player's own weapon clips (failed pre-swap), and their
		// activations must still reach the log.
		{
			constexpr uint64_t kLogDeferGraceFrames = 10;

			std::string pendingSuffix;
			{
				std::shared_lock slock(s_clipRealPathStateMutex);
				auto pit = s_pendingActivateLog.find(a_this);
				if (pit != s_pendingActivateLog.end()) {
					const bool resolved = s_clipRealPathAuthoritative.contains(a_this);
					const auto curFrame = s_currentFrame.load(std::memory_order_relaxed);
					const bool graceOver = curFrame >= pit->second.frame + kLogDeferGraceFrames;
					if (resolved || graceOver) pendingSuffix = pit->second.suffix;
				}
			}
			if (!pendingSuffix.empty()) {
				{
					std::unique_lock slock(s_clipRealPathStateMutex);
					s_pendingActivateLog.erase(a_this);
				}
				if (AnimationLog::GetSingleton()->IsEnabled()) {
					// Leaf-validated against the entry's own animation — the
					// cache may hold a path for a different animation by now.
					const auto displayPath = DisplayPathForEntry(a_this, pendingSuffix);
					AnimationLog::GetSingleton()->AddEntry(
						AnimationLog::EventType::kActivate,
						ResolveLogRefr(a_this, a_context), pendingSuffix, "", "",
						displayPath, ClassifyClipPerspective(a_this, displayPath));
				}
			}
		}

		// Global runtime toggle (Settings > General > "Enabled"): when off, do
		// no replacement work at all (the anim-log flush above stays active —
		// the log is a monitoring tool, the toggle governs replacement). The
		// bulk vanilla restore ran when the box was unticked
		// (PerformGlobalDisableRestore, game thread); this per-clip check
		// catches stragglers that still hold our clone — e.g. a clip that
		// swapped on the very frame of the toggle, or one whose original was
		// unreadable during the bulk pass.
		if (!Settings::GetSingleton()->bEnabled) {
			auto** slot = a_this->GetAnimationSlot();
			if (slot && *slot && AnimationCache::GetSingleton()->IsOurReplacement(*slot)) {
				if (auto* orig = GetValidOriginal(a_this)) {
					*slot = orig;
					RestoreClipTriggers(a_this);
					static int s_disableRestoreLog = 0;
					if (s_disableRestoreLog < 20) {
						logger::info("[OAR-Disable] Straggler clip {:X} restored to vanilla",
							reinterpret_cast<uintptr_t>(a_this));
						s_disableRestoreLog++;
					}
				}
			}
			return;
		}

		// Per-clip bypass: if pre-swap failed in Activate, the animation control was
		// built from a different animation than what we'd swap in. Skip all OAR logic
		// to prevent struct mismatch crashes.
		{
			std::shared_lock lock(s_bypassMutex);
			if (s_bypassSet.find(a_this) != s_bypassSet.end()) {
				return;
			}
		}

		auto* cache = AnimationCache::GetSingleton();

		auto* currentAnim = a_this->GetAnimation();
		if (currentAnim && cache->GetGameAnimVtable() == 0) {
			auto vtbl = *reinterpret_cast<uintptr_t*>(currentAnim);
			cache->SetVtableFromGame(vtbl);
			logger::info("[OAR] Captured game hkaAnimation vtable: {:X} (from Update)", vtbl);
		}

		// Look up the cached suffix (cached during Activate when animationName is still valid)
		std::string suffix;
		{
			std::shared_lock lock(s_clipSuffixMutex);
			auto it = s_clipSuffixCache.find(a_this);
			if (it != s_clipSuffixCache.end()) {
				suffix = it->second;
			}
		}

		if (suffix.empty()) {
			// Direct path matching first: the per-frame poll may have already
			// resolved this clip's real path even though Activate was missed.
			suffix = DirectSuffixFromCachedPath(a_this);
			if (suffix.empty()) {
				const char* clipName = a_this->animationName.data();
				if (clipName && reinterpret_cast<uintptr_t>(clipName) > 0x10000 && clipName[0] != '\0') {
					suffix = ResolveOrLeafFallback(ExtractAnimSuffix(std::string(clipName)));
				}
			}
			// Backfill the cache so Generate can find this clip's suffix later.
			// Clips that missed Activate (already active at hook install time) are
			// caught here.
			if (!suffix.empty()) {
				std::unique_lock lock(s_clipSuffixMutex);
				s_clipSuffixCache[a_this] = suffix;
			}
		}

		if (suffix.empty()) return;

		// ===== Entry grace window =====
		// The decisive condition evaluation at play entry can read racy game
		// state: on a full-auto weapon the empty reload auto-triggers the same
		// instant the last round fires, and CurrentMagazineAmmo still reads
		// non-zero at that instant — so 'NOT CurrentMagazineAmmo == 0' falsely
		// PASSES and a non-interruptible tactical-reload replacement replays
		// over what should be the vanilla empty reload (MP7A2, 2026-08-19 log:
		// eval '?->true evalFalse=0' at reload entry, ammo verifiably 0 four
		// frames later). During the first moments of a SINGLE_PLAY clip the
		// non-interruptible/play-once locks therefore must NOT suppress
		// re-evaluation: a flip inside this window can only mean the entry
		// read was stale (ammo cannot legitimately change during a reload —
		// firing is impossible mid-reload), and the conditions-failed restore
		// this early in a play is field-proven clean (Dragunov empty #1, MP7
		// empty #2: full native tail annotations). Looping and user-controlled
		// clips are excluded: loop wraps restart localTime every pass and
		// would turn this into a permanent re-eval (SCAR dry-fire history).
		// Window kept SHORT: with CurrentMagazineAmmo now reading the graph's
		// LoadedAmmoCount first (frame-0 accurate), this is only the safety
		// net for graphs without the variable and for other racy entry reads;
		// a wider window would erode non-interruptible semantics for
		// legitimately fast condition flips. localTime-based, so frame
		// hitches at play start cannot close it early.
		const bool inEntryGrace =
			(a_this->mode == RE::MODE_SINGLE_PLAY) &&
			(a_this->GetLocalTime() < 0.15f);

		// ===== Vanilla annotation backup driver =====
		// Fires the armed missing annotations of an UN-replaced play at their
		// authored times (see VanillaAnnotBackup). Passive while nested inside
		// an outer graph notify, mirroring the replacement annotation tracker.
		if (s_vanillaAnnotCount.load(std::memory_order_relaxed) > 0 &&
			s_notifyAnimGraphDepth == 0) {
			bool vbFound = false;
			float vbPrevT = 0.f;
			int32_t vbLastFired = -1;
			size_t vbTotal = 0;
			{
				std::shared_lock lock(s_vanillaAnnotMutex);
				auto it = s_vanillaAnnotMap.find(a_this);
				if (it != s_vanillaAnnotMap.end()) {
					vbFound = true;
					vbPrevT = it->second.prevT;
					vbLastFired = it->second.lastFired;
					vbTotal = it->second.entries.size();
				}
			}
			if (vbFound) {
				const float vbCurT = a_this->GetLocalTime();
				if (vbCurT < vbPrevT - 0.01f) {
					// Time went backwards: new play (or loop wrap) — this backup
					// belonged to the previous play. Drop it; the restore path
					// re-arms if the new play needs one.
					std::unique_lock lock(s_vanillaAnnotMutex);
					if (s_vanillaAnnotMap.erase(a_this)) {
						s_vanillaAnnotCount.fetch_sub(1, std::memory_order_relaxed);
					}
				} else if (vbCurT > vbPrevT) {
					// Same catch-up cap as the replacement tracker: a jump larger
					// than this is a seek/hitch — advance without dumping sounds.
					const bool fire = (vbCurT - vbPrevT) <= 0.30f;
					std::vector<std::string> vbSounds;
					std::vector<std::string> vbEvents;
					bool vbDone = false;
					{
						std::unique_lock lock(s_vanillaAnnotMutex);
						auto it = s_vanillaAnnotMap.find(a_this);
						if (it != s_vanillaAnnotMap.end()) {
							auto& vb = it->second;
							const int32_t total = static_cast<int32_t>(vb.entries.size());
							for (int32_t i = vb.lastFired + 1; i < total; ++i) {
								if (vb.entries[i].time > vbCurT) break;
								if (fire) {
									static constexpr const char* kSp = "SoundPlay.";
									if (vb.entries[i].text.size() > 10 &&
										_strnicmp(vb.entries[i].text.c_str(), kSp, 10) == 0) {
										vbSounds.push_back(vb.entries[i].text);
									} else {
										vbEvents.push_back(vb.entries[i].text);
									}
								}
								vb.lastFired = i;
							}
							vb.prevT = vbCurT;
							if (vb.lastFired + 1 >= total) {
								s_vanillaAnnotMap.erase(it);
								s_vanillaAnnotCount.fetch_sub(1, std::memory_order_relaxed);
								vbDone = true;
							}
						}
					}
					if (!vbSounds.empty() || !vbEvents.empty()) {
						auto* vbRefr = GetRefrFromContext(a_context);
						if (!vbRefr) vbRefr = RE::PlayerCharacter::GetSingleton();
						if (vbRefr) {
							for (auto& s : vbSounds) {
								PlaySoundDirect(s.c_str() + 10, vbRefr);
								static std::atomic<int> s_vbFireLog{ 0 };
								if (s_vbFireLog.fetch_add(1, std::memory_order_relaxed) < 60) {
									logger::info("[OAR-VanillaBackup] Fired '{}' (clipGen={:X}, t={:.3f})",
										s, reinterpret_cast<uintptr_t>(a_this), vbCurT);
								}
							}
							if (!vbEvents.empty()) {
								QueueCustomEvents(vbRefr, vbEvents, "vanilla-backup");
								static std::atomic<int> s_vbEvtLog{ 0 };
								for (auto& e : vbEvents) {
									if (s_vbEvtLog.fetch_add(1, std::memory_order_relaxed) < 60) {
										logger::info("[OAR-VanillaBackup] Queued event '{}' (clipGen={:X}, t={:.3f})",
											e, reinterpret_cast<uintptr_t>(a_this), vbCurT);
									}
								}
							}
						}
					}
					(void)vbDone;
					(void)vbLastFired;
					(void)vbTotal;
				}
			}
		}

		// ===== Per-play annotation integrity check =====
		// NON-NEGOTIABLE CONTRACT: every annotation of the animation actually
		// playing must fire at its authored time — for donor/vanilla plays and
		// for replacement plays whose annotations come from the original file
		// (Replace Annotations unticked). The engine's play-local trigger data
		// can be built wrong at _Activate (stale clone bound in the shared
		// binding → triggers past the clone's end never exist for the play),
		// so once per play, verify the live trigger array against the
		// authoritative annotation source and arm the manual backup for
		// anything missing. Plays where OAR NULLs the triggers (Replace
		// Annotations ticked) are exempt — manual firing is already the
		// authority there, and the trigger install below disarms any backup
		// this check registered earlier in the same play.
		if (s_gameFullyLoaded.load() && s_notifyAnimGraphDepth == 0 &&
			a_this->GetLocalTime() >= 0.f) {
			bool needCheck = false;
			{
				// Once per PLAY, not per generator: a localTime regression on
				// the same generator is a re-entered play and re-runs the check.
				const float icT = a_this->GetLocalTime();
				std::unique_lock lock(s_annotIntegrityMutex);
				auto [it, inserted] = s_annotIntegrityLastT.try_emplace(a_this, icT);
				if (inserted) {
					needCheck = true;
				} else {
					if (icT < it->second - 0.05f) needCheck = true;
					it->second = icT;
				}
			}
			if (needCheck) {
				bool triggersOurs = false;
				{
					std::shared_lock tLock(s_triggersBackupMutex);
					auto tIt = s_triggersBackup.find(a_this);
					triggersOurs = (tIt != s_triggersBackup.end() && tIt->second.nulled);
				}
				if (!triggersOurs) {
					if (auto** icSlot = a_this->GetAnimationSlot(); icSlot && *icSlot) {
						auto* icCache = AnimationCache::GetSingleton();
						RE::hkaAnimation* icSource = *icSlot;
						if (icCache->IsOurReplacement(icSource)) {
							// Clone in the slot with native triggers: the contract
							// says the ORIGINAL's annotations fire (Replace
							// Annotations off, or the swap just hasn't decided
							// yet — the install path disarms if it takes over).
							icSource = icCache->GetOriginalFromReplacement(icSource);
							if (icSource && (IsBadReadPtr(icSource, sizeof(uintptr_t)) ||
								!IsPlausibleGameAnimVtable(*reinterpret_cast<uintptr_t*>(icSource)))) {
								icSource = nullptr;
							}
						}
						if (icSource) {
							ArmVanillaAnnotationBackup(a_this, icSource, a_this->GetLocalTime());
						}
					}
				}
			}
		}

		// ===== Direct-path defer gate =====
		// For the first frames after Activate the cached suffix is only the
		// authored/leaf-derived GUESS (the subgraph walk fails during graph
		// transitions). Matching against that guess can install the WRONG
		// submod's file: a bare-leaf registration (files at the submod root)
		// hijacks folder-scoped animations — e.g. '1911 Idle Empty' registered
		// under 'wpnidleready' replacing a clip whose real path is
		// '1911anims\wpnidleready.hkx' (which has no registered replacement).
		// Worse, once installed, EnsureDirectSuffixForClip refuses to re-key
		// the clip (by design — re-keying mid-replacement desyncs restore
		// state), so the wrong file stays locked in permanently.
		//
		// The per-frame poll can't help here: it only sees a fresh clip at the
		// END of the frame, after this Update already ran. And Activate-time
		// attribution (PlayerGraphIndexForClip) fails while the graph is
		// mid-transition. So do the work synchronously HERE — at Update time
		// the clip's nodeInfo is assigned, so both the player-graph membership
		// test and the subgraph path resolution succeed:
		//   1. Attribute the clip to a player root graph (retry briefly).
		//   2. Player clip: resolve the REAL path now and match against its
		//      exact suffix; if resolution fails (graph still rebuilding),
		//      hold off for a short frame budget, then fall back to leaf
		//      matching (the documented fallback for unresolvable clips).
		//   3. Non-player clip (attribution exhausted): keep instant leaf
		//      matching — NPC clips have no swap array to resolve against.
		// Skipped when one of OUR replacements is already installed: the
		// suffix was validated on a previous frame, and the maintenance logic
		// below (annotation firing, restore-on-condition-fail) must keep
		// running. Also skipped when no replacement is registered for the
		// current guess — the match below exits as NoMatch anyway, so the
		// attribution/resolution cost would buy nothing.
		if (Settings::GetSingleton()->bDirectPathMatching) {
			bool matchPossible = false;
			{
				std::shared_lock rlock(s_nameLookupMutex);
				if (suffix.size() > 6 && suffix.compare(0, 6, "multi:") == 0) {
					matchPossible = s_leafToFullSuffixes.find(suffix.substr(6)) != s_leafToFullSuffixes.end();
				} else {
					matchPossible = s_suffixToInfos.find(suffix) != s_suffixToInfos.end();
				}
			}
			bool authoritative = false;
			if (matchPossible) {
				std::shared_lock slock(s_clipRealPathStateMutex);
				authoritative = s_clipRealPathAuthoritative.contains(a_this);
			}
			if (matchPossible && !authoritative) {
				auto** slotNow = a_this->GetAnimationSlot();
				const bool ourInstalled = slotNow && *slotNow && cache->IsOurReplacement(*slotNow);
				if (!ourInstalled) {
					constexpr uint16_t kDirectPathDeferFrames = 20;  // resolution budget (player clips)
					constexpr uint16_t kAttributionAttempts = 3;     // attribution budget (unknown clips)

					uint16_t attemptsNow = 0;
					{
						std::shared_lock slock(s_clipRealPathStateMutex);
						auto ait = s_clipRealPathAttempts.find(a_this);
						if (ait != s_clipRealPathAttempts.end()) attemptsNow = ait->second;
					}

					bool isPlayerClip = false;
					{
						std::shared_lock plock(s_playerClipMutex);
						isPlayerClip = s_playerClipGraph.find(a_this) != s_playerClipGraph.end();
					}
					if (!isPlayerClip && attemptsNow < kAttributionAttempts) {
						if (const auto gi = PlayerGraphIndexForClip(a_this, a_context); gi >= 0) {
							std::unique_lock plock(s_playerClipMutex);
							s_playerClipGraph[a_this] = static_cast<uint8_t>(gi);
							isPlayerClip = true;
						}
					}

					if (isPlayerClip) {
						// Resolve the real path NOW — the decision below must be
						// made against the engine's ground truth, not the guess.
						// The walk failing does NOT mean the truth is unknown:
						// another (pooled) clip instance of the same underlying
						// binding may already have resolved it — inherit that and
						// promote it to a full authoritative resolution, identical
						// to a walk success. This keeps PATH matching in charge
						// across clip pool recycling instead of burning defer
						// frames and dropping to the leaf-match guess (which
						// installed 'p890\fasthands\xmaglrg\wpnreload' for a
						// clip whose real file was 'p890\wpnreload').
						auto realPath = ResolveClipPathFromSubgraph(a_this, a_context);
						bool inherited = false;
						if (realPath.empty()) {
							std::string leafGuess = suffix;
							if (leafGuess.rfind("multi:", 0) == 0) leafGuess = leafGuess.substr(6);
							realPath = InheritedBindingPathForClip(a_this, SubgraphGetLeaf(leafGuess.c_str()));
							inherited = !realPath.empty();
						}
						if (!realPath.empty()) {
							{
								std::unique_lock plock(s_clipRealPathMutex);
								s_clipRealPathCache[a_this] = realPath;
							}
							{
								std::unique_lock slock(s_clipRealPathStateMutex);
								s_clipRealPathAuthoritative.insert(a_this);
								s_clipRealPathAttempts.erase(a_this);
							}
							EnsureDirectSuffixForClip(a_this, realPath);
							if (auto exact = ExtractAnimSuffix(realPath); !exact.empty()) {
								// Match against the REAL path from this frame on — unless
								// a Leaf Matching submod claims this filename, which
								// converts to multi mode (flagged submod probed first,
								// the real path's candidates as fallback).
								suffix = ApplyLeafOverride(exact);
							}
							if (inherited) {
								static std::atomic<int> s_inheritPromoteLog{ 0 };
								if (s_inheritPromoteLog.fetch_add(1, std::memory_order_relaxed) < 20) {
									logger::info("[OAR-DirectPath] Clip {:X} inherited binding path '{}' (subgraph walk unavailable)",
										reinterpret_cast<uintptr_t>(a_this), realPath);
								}
							}
						} else {
							std::unique_lock slock(s_clipRealPathStateMutex);
							auto& attempts = s_clipRealPathAttempts[a_this];
							if (attempts < kDirectPathDeferFrames) {
								++attempts;
								return;
							}
						}
					} else if (attemptsNow < kAttributionAttempts) {
						// Attribution unknown (graph mid-transition) — hold off
						// briefly rather than risk a wrong leaf-guess install.
						std::unique_lock slock(s_clipRealPathStateMutex);
						++s_clipRealPathAttempts[a_this];
						return;
					}
					// Attribution exhausted: non-player clip — fall through to
					// normal (leaf) matching.
				}
			}
		}

		// Diagnostic: log unique suffixes
		{
			static std::unordered_set<std::string> s_loggedSuffixes;
			static std::shared_mutex s_loggedSuffixMutex;
			std::shared_lock slock(s_loggedSuffixMutex);
			bool isNew = s_loggedSuffixes.find(suffix) == s_loggedSuffixes.end();
			slock.unlock();
			if (isNew) {
				std::unique_lock ulock(s_loggedSuffixMutex);
				if (s_loggedSuffixes.insert(suffix).second) {
					bool found = s_suffixToInfos.find(suffix) != s_suffixToInfos.end();
					logger::info("[OAR-Match] suffix='{}' match={}", suffix, found);
				}
			}
		}

		// Handle multi-match mode: suffix starts with "multi:" meaning multiple candidate
		// suffixes share the same leaf name. We'll evaluate conditions for each.
		bool isMultiMatch = (suffix.size() > 6 && suffix.substr(0, 6) == "multi:");
		std::string resolvedSuffix = suffix;
		std::vector<ReplacementAnimFileInfo*> const* candidatesPtr = nullptr;

		// Orphan recovery (mid-session save load): when a clip still holds our
		// clone in its slot but the condition probe resolves no candidate — the
		// save load wiped the binding-identity map, and the probe alone can't
		// re-identify the variant — we re-identify the clip from the clone the
		// engine is actually playing (ground truth) and fire THAT submod's
		// annotations instead of returning early and letting the original mod's
		// annotations leak. orphanAnnotOnly means "clone already installed;
		// don't rebuild/swap, just correct the annotations + triggers".
		SubMod* orphanRecoverOwner = nullptr;
		bool orphanAnnotOnly = false;
		// Set by the Leaf Matching pre-pass when it claims a suffix that is NOT
		// the clip's own registered path (pure filename match): the claimed
		// suffix's candidate vector belongs to ANOTHER path, so only the flagged
		// submod that made the claim may win — priority order over that vector
		// must not hand the play to a path-scoped submod that never matched
		// this clip (field case: 'M4A1 - Melee Variants' hijacking a foreign
		// weapon's claim, 2026-08-16).
		SubMod* leafForcedSubMod = nullptr;

		std::shared_lock lock(s_nameLookupMutex);

		if (isMultiMatch) {
			std::string leafName = suffix.substr(6);
			auto leafIt = s_leafToFullSuffixes.find(leafName);
			if (leafIt == s_leafToFullSuffixes.end() || leafIt->second.empty()) {
				return;
			}

			RE::TESObjectREFR* multiRefr = GetRefrFromContext(a_context);
			if (!multiRefr) multiRefr = RE::PlayerCharacter::GetSingleton();

			// ===== Leaf Matching pre-pass =====
			// Submods with the Leaf Matching flag match this clip by FILENAME
			// alone and outrank everything below it — the binding-identity
			// ground truth and the most-specific-path probe both only decide
			// which PATH-scoped candidate applies, and this flag exists to beat
			// path scoping. A locked (non-interruptible, active) path submod
			// still wins: yanking it mid-play is what non-interruptible forbids.
			// The clip's REAL registered suffix, when its identity is known —
			// authoritative poll resolution first, binding inheritance second
			// (see RealSuffixForClip). Shared by the Leaf Matching pre-pass (a
			// flagged submod usually mirrors SEVERAL paths sharing the filename;
			// on its own weapon the file whose registered path matches the clip
			// must play, and claims elsewhere are FOREIGN) and by the
			// binding-identity safety net below.
			std::string exactSuffix = RealSuffixForClip(a_this, leafName);

			{
				SubMod* lockedActive = nullptr;
				{
					SubMod* active = ValidatedActiveSubMod(a_this);
					// Entry grace: no lock during the play's first moments — the
					// pre-pass must re-evaluate conditions so a racy entry read
					// (see inEntryGrace) self-corrects on leaf-matched clips too.
					if (active && !active->IsInterruptible() && !active->IsDisabled() &&
						!inEntryGrace) {
						lockedActive = active;
					}
				}
				auto ovIt = s_leafOverrideSuffixes.find(leafName);
				if (ovIt != s_leafOverrideSuffixes.end() &&
					(!lockedActive || lockedActive->GetLeafMatching())) {
					auto tryClaim = [&](const std::string& a_ovSuffix) -> bool {
						auto candIt = s_suffixToInfos.find(a_ovSuffix);
						if (candIt == s_suffixToInfos.end()) return false;
						for (auto* info : candIt->second) {
							if (!info || !info->parentSubMod) continue;
							if (!info->parentSubMod->GetLeafMatching()) continue;
							if (info->parentSubMod->IsDisabled()) continue;
							if (!ClaimSkeletonAllowed(a_this, info)) continue;
							bool pass = false;
							if (lockedActive) {
								// Locked leaf-matching submod stays selected without
								// re-evaluating conditions (non-interruptible semantics).
								pass = (info->parentSubMod == lockedActive);
							} else {
								auto* cs = info->parentSubMod->GetConditionSet();
								pass = (!cs || cs->IsEmpty());
								if (!pass && multiRefr) {
									try {
										pass = info->parentSubMod->EvaluateConditions(multiRefr, a_this);
									} catch (...) { pass = false; }
								}
							}
							if (pass) {
								resolvedSuffix = a_ovSuffix;
								candidatesPtr = &candIt->second;
								if (a_ovSuffix != exactSuffix) {
									leafForcedSubMod = info->parentSubMod;
								}
								return true;
							}
						}
						return false;
					};

					// Selection order: the clip's own registered suffix first (when
					// it is itself flagged), then the override list (priority desc,
					// base/least-specific file first).
					bool claimed = false;
					if (!exactSuffix.empty() &&
						std::ranges::find(ovIt->second, exactSuffix) != ovIt->second.end()) {
						claimed = tryClaim(exactSuffix);
					}
					if (!claimed) {
						for (auto& ovSuffix : ovIt->second) {
							if (ovSuffix == exactSuffix) continue;  // already tried
							if (tryClaim(ovSuffix)) break;
						}
					}
					if (candidatesPtr) {
						// Log once per (clip, claimed suffix) — the per-frame
						// repeat of an unchanged claim burned the old absolute
						// cap in one play and blinded later diagnosis
						// (Cryolator 2026-08-19).
						static std::mutex s_leafOvLogMutex;
						static std::unordered_map<RE::hkbClipGenerator*, std::string> s_leafOvLastClaim;
						bool logClaim = false;
						{
							std::lock_guard lgLock(s_leafOvLogMutex);
							auto [it, inserted] = s_leafOvLastClaim.try_emplace(a_this, resolvedSuffix);
							if (inserted || it->second != resolvedSuffix) {
								it->second = resolvedSuffix;
								logClaim = true;
							}
						}
						if (logClaim) {
							logger::info("[OAR-LeafMatch] leaf='{}' claimed by leaf-matching suffix='{}' (exact='{}', foreign={}, clipGen={:X})",
								leafName, resolvedSuffix, exactSuffix, leafForcedSubMod != nullptr,
								reinterpret_cast<uintptr_t>(a_this));
						}
					}
				}
			}

			// Binding-identity safety net. The defer gate normally promotes an
			// inherited binding path BEFORE the suffix ever reaches here as
			// "multi:", but some clips arrive in multi mode anyway (gate
			// skipped while our replacement occupies the slot, non-player
			// attribution, stale multi suffix cached from a previous frame).
			// If the real path for this clip is known, that is ground truth —
			// the condition probe below cannot discriminate variants whose
			// submods share the same conditions (all gated on IsEquipped), and
			// its most-specific-first order then installs the WRONG variant's
			// file. Uses the shared exactSuffix (authoritative poll resolution
			// first, leaf-validated binding inheritance second).
			bool bindingKnown = false;
			if (!candidatesPtr && !exactSuffix.empty()) {
				bindingKnown = true;
				auto candIt = s_suffixToInfos.find(exactSuffix);
				if (candIt != s_suffixToInfos.end()) {
					resolvedSuffix = exactSuffix;
					candidatesPtr = &candIt->second;
				}
				// Registered or not, the identity is settled: when no
				// replacement exists under the real suffix, fall through
				// to the no-candidate restore below rather than letting
				// the probe pick a different variant's file.
				static std::atomic<int> s_bindInheritLog{ 0 };
				if (s_bindInheritLog.fetch_add(1, std::memory_order_relaxed) < 20) {
					logger::info("[OAR-MultiMatch] leaf='{}' settled real suffix='{}' (registered={})",
						leafName, exactSuffix, candidatesPtr != nullptr);
				}
			}

			// Evaluate each candidate suffix's conditions, most-specific (longest) first
			if (!candidatesPtr && !bindingKnown)
			for (auto& candidateSuffix : leafIt->second) {
				auto candIt = s_suffixToInfos.find(candidateSuffix);
				if (candIt == s_suffixToInfos.end()) continue;

				for (auto* info : candIt->second) {
					if (!info || !info->parentSubMod) continue;
					if (info->parentSubMod->IsDisabled()) continue;
					if (!info->parentSubMod->GetConditionSet()) {
						resolvedSuffix = candidateSuffix;
						candidatesPtr = &candIt->second;
						break;
					}
					if (multiRefr) {
						try {
							if (info->parentSubMod->EvaluateConditions(multiRefr, a_this)) {
								resolvedSuffix = candidateSuffix;
								candidatesPtr = &candIt->second;
								break;
							}
						} catch (...) { continue; }
					}
				}
				if (candidatesPtr) break;
			}

			// Orphan recovery, tried BEFORE the restore/leave fallback below.
			// If the slot still holds one of our clones and the cache can name
			// the suffix + owning SubMod it belongs to, that identity is ground
			// truth for what is visibly playing. Validate the owner against the
			// LIVE candidate infos for this leaf (never dereference a possibly
			// stale SubMod*), then resolve to it so the correct annotations fire.
			if (!candidatesPtr) {
				auto** oSlot = a_this->GetAnimationSlot();
				auto* oCache = AnimationCache::GetSingleton();
				if (oSlot && *oSlot && oCache->IsOurReplacement(*oSlot)) {
					std::string recSuffix;
					const void* recOwner = nullptr;
					if (oCache->GetReplacementIdentity(*oSlot, recSuffix, recOwner) && recOwner) {
						for (auto& candSfx : leafIt->second) {
							auto ci = s_suffixToInfos.find(candSfx);
							if (ci == s_suffixToInfos.end()) continue;
							for (auto* info : ci->second) {
								if (!info || !info->parentSubMod) continue;
								if (static_cast<const void*>(info->parentSubMod) != recOwner) continue;
								if (info->parentSubMod->IsDisabled()) continue;
								resolvedSuffix = candSfx;
								candidatesPtr = &ci->second;
								orphanRecoverOwner = info->parentSubMod;
								orphanAnnotOnly = true;
								break;
							}
							if (candidatesPtr) break;
						}
						if (orphanAnnotOnly) {
							static int s_orphanRecLog = 0;
							if (s_orphanRecLog < 20) {
								logger::info("[OAR-OrphanRecover] leaf='{}' re-identified stuck clone as suffix='{}' owner={:X} — firing correct annotations",
									leafName, resolvedSuffix, reinterpret_cast<uintptr_t>(orphanRecoverOwner));
								s_orphanRecLog++;
							}
						}
					}
				}
			}

			if (!candidatesPtr) {
				// No candidate passed conditions — but our previous replacement may
				// still be in the animation slot. Restore the original if needed.
				auto** animSlot = a_this->GetAnimationSlot();
				if (animSlot && *animSlot) {
					auto* cache = AnimationCache::GetSingleton();
					if (cache->IsOurReplacement(*animSlot)) {
						// Use validated access — returns nullptr if pointer is stale/freed.
						// This is the fix for the crash where weapon switch freed the old
						// animation but we still tried to write the dangling pointer into the slot.
						RE::hkaAnimation* originalToRestore = GetValidOriginal(a_this);

						// Fallback: ask the cache (gameOriginal may also be stale after weapon switch)
						if (!originalToRestore) {
							auto* recovered = cache->GetOriginalFromReplacement(*animSlot);
							if (recovered && !IsBadReadPtr(recovered, sizeof(uintptr_t))) {
								auto vtbl = *reinterpret_cast<uintptr_t*>(recovered);
								if (IsPlausibleGameAnimVtable(vtbl)) {
									originalToRestore = recovered;
								}
							}
						}

						if (originalToRestore) {
							*animSlot = originalToRestore;
							RestoreClipTriggers(a_this);
							{
								std::unique_lock olock(s_originalAnimMutex);
								s_originalAnimMap[a_this] = originalToRestore;
							}
							static int s_multiRestoreLog = 0;
							if (s_multiRestoreLog < 30) {
								logger::info("[OAR-MultiMatch] leaf='{}' - restoring validated original (conditions no longer met)", leafName);
								s_multiRestoreLog++;
							}
						} else {
							// Cannot restore safely — leave our clone in the slot.
							// The clone's memory is heap-stable (never freed until cache clear),
							// so it's always safe even if the animation doesn't match the weapon.
							// This is much better than writing a stale pointer and crashing.
							static int s_leaveLog = 0;
							if (s_leaveLog < 30) {
								logger::warn("[OAR-MultiMatch] leaf='{}' - original stale, leaving clone in slot (safe fallback)", leafName);
								s_leaveLog++;
							}
						}
					}
				}

				// Also clean up active replacement tracking
				RE::TESObjectREFR* cleanRefr = GetRefrFromContext(a_context);
				if (!cleanRefr) cleanRefr = RE::PlayerCharacter::GetSingleton();
				uint32_t cleanActorID = cleanRefr ? cleanRefr->GetFormID() : 0;
				ActiveReplacementTracker::GetSingleton()->Remove(cleanActorID, leafName);
				{
					std::unique_lock slock(s_activeReplacementSuffixMutex);
					s_activeReplacementSuffixes.erase(leafName);
				}
				{
					std::unique_lock alock(s_activeReplacementActorMutex);
					auto it = s_activeReplacementByActor.find(cleanActorID);
					if (it != s_activeReplacementByActor.end()) {
						it->second.erase(leafName);
					}
				}
				return;
			}

			static int s_multiLog = 0;
			if (s_multiLog < 20) {
				logger::info("[OAR-MultiMatch] leaf='{}' resolved to suffix='{}'",
					leafName, resolvedSuffix);
				s_multiLog++;
			}
		} else {
			auto infoIt = s_suffixToInfos.find(suffix);
			if (infoIt == s_suffixToInfos.end()) {
				static std::atomic<int> s_noMatchLogCount{ 0 };
				if (s_noMatchLogCount.fetch_add(1, std::memory_order_relaxed) < 30) {
					logger::info("[OAR-NoMatch] suffix='{}' has no registered replacement", suffix);
				}
				return;
			}
			candidatesPtr = &infoIt->second;
		}
		const auto& candidates = *candidatesPtr;

		auto** animSlot = a_this->GetAnimationSlot();
		if (!animSlot || !*animSlot) {
			// Normal during Activate / early Update: the control can exist before
			// its binding (and therefore the animation slot) is attached, or the
			// binding can be briefly cleared during a graph transition. Returning
			// here is correct — there is nothing to swap yet. Was a warn that
			// spammed the log on every weapon equip / reload (especially AE);
			// keep a tiny breadcrumb at info for genuine stuck cases.
			static int s_slotNullLog = 0;
			if (s_slotNullLog < 5) {
				logger::info("[OAR-SlotNull] animSlot not ready for '{}' clipGen={:X} ctrl={:X} (transient; skipped)",
					resolvedSuffix, reinterpret_cast<uintptr_t>(a_this),
					reinterpret_cast<uintptr_t>(a_this->GetAnimationControlRaw()));
				s_slotNullLog++;
			}
			return;
		}

		// Read original animation from map — uses GetValidOriginal which validates
		// IsBadReadPtr + exact vtable match before returning the pointer.
		RE::hkaAnimation* originalAnim = GetValidOriginal(a_this);
		if (!originalAnim) {
			auto* cache = AnimationCache::GetSingleton();
			RE::hkaAnimation* current = *animSlot;
			if (!cache->IsOurReplacement(current)) {
				// Current slot holds a game animation — accept any game-module
				// hkaAnimation vtable (spline / interleaved / …), not only the
				// single type captured for packfile fixups.
				if (!IsBadReadPtr(current, sizeof(uintptr_t))) {
					auto vtbl = *reinterpret_cast<uintptr_t*>(current);
					if (IsPlausibleGameAnimVtable(vtbl)) {
						originalAnim = current;
						std::unique_lock olock(s_originalAnimMutex);
						// Overwrite any stale map entry — GetValidOriginal already
						// rejected it (that's why we're here). Do not re-read the
						// map under this lock via GetValidOriginal (non-recursive).
						s_originalAnimMap[a_this] = current;
					} else {
						static int s_rejectLog = 0;
						if (s_rejectLog < 20) {
							logger::warn("[OAR-OrigReject] current={:X} vtbl={:X} not plausible game anim ('{}')",
								reinterpret_cast<uintptr_t>(current), vtbl, resolvedSuffix);
							s_rejectLog++;
						}
					}
				}
			} else {
				// Slot has our replacement — recover the true original, but validate it
				RE::hkaAnimation* recovered = cache->GetOriginalFromReplacement(current);
				if (recovered && !IsBadReadPtr(recovered, sizeof(uintptr_t))) {
					auto vtbl = *reinterpret_cast<uintptr_t*>(recovered);
					if (IsPlausibleGameAnimVtable(vtbl)) {
						originalAnim = recovered;
						std::unique_lock olock(s_originalAnimMutex);
						s_originalAnimMap[a_this] = recovered;
					}
				}
				if (!originalAnim) {
					// Opportunistic re-arm: another clip (typically the other
					// perspective's graph, or this animation re-activating
					// elsewhere) may have rebuilt a clone for this suffix since
					// the invalidation, teaching the cache the FRESH game
					// original. Adopt it so condition changes work again for
					// this orphaned clip. Guards: vtable must look like a game
					// anim, and the track count must match the clone we're
					// currently playing (rejects an original from an incompatible
					// skeleton that merely shares the suffix).
					if (RE::hkaAnimation* fresh = cache->GetGameOriginalForSuffix(resolvedSuffix);
						fresh && fresh != current && !IsBadReadPtr(fresh, 0x20) &&
						!IsBadReadPtr(current, 0x20)) {
						auto vtbl = *reinterpret_cast<uintptr_t*>(fresh);
						// hkaAnimation: +0x18 = numTransformTracks
						auto freshTracks = *reinterpret_cast<int32_t*>(
							reinterpret_cast<uint8_t*>(fresh) + 0x18);
						auto currentTracks = *reinterpret_cast<int32_t*>(
							reinterpret_cast<uint8_t*>(current) + 0x18);
						if (IsPlausibleGameAnimVtable(vtbl) && freshTracks == currentTracks) {
							originalAnim = fresh;
							std::unique_lock olock(s_originalAnimMutex);
							s_originalAnimMap[a_this] = fresh;
							static int s_rearmLog = 0;
							if (s_rearmLog < 20) {
								logger::info("[OAR-Rearm] Adopted fresh original {:X} for orphaned clip {:X} ('{}', {} tracks)",
									reinterpret_cast<uintptr_t>(fresh),
									reinterpret_cast<uintptr_t>(a_this),
									resolvedSuffix, freshTracks);
								s_rearmLog++;
							}
						}
					}
				}
				if (!originalAnim && !orphanAnnotOnly) {
					static int s_recoveryFailLog = 0;
					if (s_recoveryFailLog < 20) {
						logger::warn("[OAR-RecoveryFail] Can't recover valid original for '{}' clipGen={:X} current={:X}",
							resolvedSuffix, reinterpret_cast<uintptr_t>(a_this),
							reinterpret_cast<uintptr_t>(current));
						s_recoveryFailLog++;
					}
					return;
				}
				// orphanAnnotOnly: no valid original is expected (the game
				// original was freed by the save load). We won't rebuild or swap
				// — the clone is already in the slot — so continue to fire the
				// correct annotations for it.
			}
		}

		// Still no template to clone from — cannot swap / register track filter.
		// Log loudly; previously this fell through and silently no-op'd after
		// conditions passed (looked like "conditions met but idle empty never applied").
		// Exception: orphanAnnotOnly deliberately has no template (the clone is
		// already installed) and only needs the annotation-firing path.
		if (!originalAnim && !orphanAnnotOnly) {
			static int s_noOrigLog = 0;
			if (s_noOrigLog < 30) {
				logger::warn("[OAR-NoOriginal] No game-anim template for '{}' clipGen={:X} — skip replace this frame",
					resolvedSuffix, reinterpret_cast<uintptr_t>(a_this));
				s_noOrigLog++;
			}
			return;
		}

		// Evaluate conditions
		RE::TESObjectREFR* refr = GetRefrFromContext(a_context);
		if (!refr) refr = RE::PlayerCharacter::GetSingleton();

		{
			static std::atomic<int> s_condEvalReachCount{ 0 };
			int reachCount = s_condEvalReachCount.fetch_add(1);
			if (reachCount < 5) {
				logger::info("[OAR-CondEval] Reached condition eval for '{}' (count={}, original={:X}, animSlot={:X})",
					resolvedSuffix, reachCount,
					reinterpret_cast<uintptr_t>(originalAnim),
					reinterpret_cast<uintptr_t>(*animSlot));
			}
		}

		// "Play Once (Full Body)": once a clip has been initially evaluated and a
		// candidate SubMod has playOnceFullBody, the initial decision (replace or not)
		// is locked for the clip's entire lifetime. This prevents mid-animation
		// condition flips in BOTH directions:
		//   - replacement active  → conditions flip false → replacement stays
		//   - no replacement      → conditions flip true  → stays un-replaced
		// The set is cleaned in hkbClipGenerator_Deactivate.
		bool hasPlayOnceCandidate = false;
		for (auto* info : candidates) {
			if (info && info->parentSubMod && info->parentSubMod->GetPlayOnceFullBody() &&
				!info->parentSubMod->IsDisabled()) {
				hasPlayOnceCandidate = true;
				break;
			}
		}

		bool playOnceLocked = false;
		bool playOnceLockedResult = false;
		SubMod* playOnceLockedWinner = nullptr;
		// Entry grace: don't replay a cached play-once decision while the
		// window is open — the fresh evaluation below re-records it each
		// frame, and the LAST record when the window closes is the one that
		// locks (see the inEntryGrace comment).
		if (hasPlayOnceCandidate && !inEntryGrace) {
			std::shared_lock poLock(s_playOnceDecisionMutex);
			auto it = s_playOnceDecision.find(a_this);
			if (it != s_playOnceDecision.end()) {
				playOnceLocked = true;
				playOnceLockedResult = it->second.replace;
				playOnceLockedWinner = it->second.winner;
			}
		}

		// "Interruptible" check: if the clip currently has an active replacement from
		// a non-interruptible submod, skip condition re-evaluation but still continue
		// to the replacement path for annotation firing and trigger maintenance.
		bool skipConditionEval = false;
		SubMod* lockedSubMod = nullptr;
		{
			// Validated: a stale entry on a recycled clip must not lock in the
			// old weapon's submod (see ValidatedActiveSubMod).
			SubMod* activeSub = ValidatedActiveSubMod(a_this);
			if (activeSub && !activeSub->IsInterruptible() && !activeSub->IsDisabled()) {
				bool allowReeval = false;

				// Check if a loop/echo event allows re-evaluation
				{
					std::unique_lock leLock(s_loopEchoFlagMutex);
					auto loopIt = s_clipLoopPending.find(a_this);
					if (loopIt != s_clipLoopPending.end() && loopIt->second) {
						if (activeSub->GetReplaceOnLoop()) {
							allowReeval = true;
						}
						loopIt->second = false;
					}
					auto echoIt = s_clipEchoPending.find(a_this);
					if (echoIt != s_clipEchoPending.end() && echoIt->second) {
						if (activeSub->GetReplaceOnEcho()) {
							allowReeval = true;
						}
						echoIt->second = false;
					}
				}

				// Entry grace: keep evaluating during the play's first moments
				// so a winner decided on a racy entry read self-corrects (see
				// the inEntryGrace comment). Once the window closes the lock
				// resumes normal non-interruptible semantics for the rest of
				// the play.
				if (inEntryGrace) {
					allowReeval = true;
				}

				if (!allowReeval) {
					skipConditionEval = true;
					lockedSubMod = activeSub;
				}
			}
		}

		// Orphan recovery forces the replacement decision: the clip is already
		// playing our clone, we identified its owner from the cache, and the
		// condition probe can't help (state wiped by the save load). Reuse the
		// skip-condition-eval machinery so downstream picks winningInfo by owner.
		if (!skipConditionEval && orphanAnnotOnly && orphanRecoverOwner) {
			skipConditionEval = true;
			lockedSubMod = orphanRecoverOwner;
		}

		// A Leaf Matching claim on a foreign path: only the flagged submod may
		// win — its conditions already passed in the pre-pass this frame. Reuse
		// the locked-winner machinery so the winner loop below cannot hand the
		// play to a path-scoped submod registered under the claimed suffix.
		if (!skipConditionEval && leafForcedSubMod) {
			skipConditionEval = true;
			lockedSubMod = leafForcedSubMod;
		}

		bool shouldReplace = false;
		ReplacementAnimFileInfo* winningInfo = nullptr;
		int totalCands = 0, disabledCands = 0, evalFalseCands = 0;
		int noCondCands = 0;

		if (skipConditionEval) {
			// Non-interruptible active replacement — skip condition evaluation entirely.
			// Assume replacement stays active; find the matching info for annotation/trigger logic.
			shouldReplace = true;
			for (auto* info : candidates) {
				if (info && info->parentSubMod == lockedSubMod) {
					winningInfo = info;
					break;
				}
			}
		} else if (playOnceLocked) {
			shouldReplace = playOnceLockedResult;
			if (shouldReplace) {
				// Replay the RECORDED winner. Re-deriving it here (the old code
				// took the first playOnceFullBody candidate) handed the play to
				// a lower-priority play-once submod even when a higher-priority
				// submod had won the initial evaluation.
				for (auto* info : candidates) {
					if (info && info->parentSubMod && info->parentSubMod == playOnceLockedWinner &&
						!info->parentSubMod->IsDisabled()) {
						winningInfo = info;
						break;
					}
				}
				// Recorded winner gone (disabled mid-play): fall back to the
				// old first-play-once-candidate pick rather than yanking the
				// replacement mid-animation.
				if (!winningInfo) {
					for (auto* info : candidates) {
						if (info && info->parentSubMod && info->parentSubMod->GetPlayOnceFullBody() &&
							!info->parentSubMod->IsDisabled()) {
							winningInfo = info;
							break;
						}
					}
				}
			}
		} else {
			for (auto* info : candidates) {
				if (!info || !info->parentSubMod) continue;
				++totalCands;
				if (info->parentSubMod->IsDisabled()) { ++disabledCands; continue; }
				if (!ClaimSkeletonAllowed(a_this, info)) { ++disabledCands; continue; }
				auto* cs = info->parentSubMod->GetConditionSet();
				if (!cs || cs->IsEmpty()) { shouldReplace = true; winningInfo = info; ++noCondCands; break; }
				if (!refr) continue;
				try {
					if (info->parentSubMod->EvaluateConditions(refr, a_this)) { shouldReplace = true; winningInfo = info; break; }
					++evalFalseCands;
				} catch (...) { continue; }
			}

			// Record the initial decision for playOnceFullBody candidates —
			// but only when the outcome actually involves play-once semantics:
			//   - a play-once submod WON: freeze that winner for the play.
			//   - NOTHING won: freeze "no replacement" so a play-once candidate
			//     cannot kick in mid-animation when conditions flip true.
			// A non-play-once winner is deliberately NOT locked: its own
			// interruptible setting governs re-evaluation, and locking it used
			// to hand later updates to the wrong submod (see PlayOnceDecision).
			if (hasPlayOnceCandidate) {
				if (shouldReplace && winningInfo && winningInfo->parentSubMod &&
					winningInfo->parentSubMod->GetPlayOnceFullBody()) {
					std::unique_lock poLock(s_playOnceDecisionMutex);
					s_playOnceDecision[a_this] = { true, winningInfo->parentSubMod };
				} else if (!shouldReplace) {
					std::unique_lock poLock(s_playOnceDecisionMutex);
					s_playOnceDecision[a_this] = { false, nullptr };
				}
			}
		}

		// NOTE (2026-08-18): a "redirect force" block lived here briefly — it
		// forced the FileRedirect submod as the winner when conditions failed,
		// on the premise that the engine had loaded the submod's file as the
		// original. The premise was wrong for archive-shipped animations: the
		// CreateFileW redirect never fires for BA2 loads (zero runtime
		// 'FILE REDIRECT:' lines in the field), so the engine original really
		// is the vanilla animation and a conditions-failed play must stay fully
		// vanilla. The tail-annotation drop that motivated it was the
		// stale-clone control build, fixed by the Activate catch-all restore.

		// Per-clip transition logging: log whenever shouldReplace flips for this clip
		{
			static std::shared_mutex s_lastShouldReplaceMutex;
			static std::unordered_map<RE::hkbClipGenerator*, bool> s_lastShouldReplace;
			bool prevKnown = false;
			bool prev = false;
			{
				std::shared_lock slock(s_lastShouldReplaceMutex);
				auto it = s_lastShouldReplace.find(a_this);
				if (it != s_lastShouldReplace.end()) { prevKnown = true; prev = it->second; }
			}
		if (!prevKnown || prev != shouldReplace) {
			std::unique_lock ulock(s_lastShouldReplaceMutex);
			s_lastShouldReplace[a_this] = shouldReplace;
			ulock.unlock();

			static std::atomic<int> s_transitionLogCount{ 0 };
			int transCount = s_transitionLogCount.fetch_add(1, std::memory_order_relaxed);
			if (transCount < 50) {
				std::string winnerName = (winningInfo && winningInfo->parentSubMod)
					? winningInfo->parentSubMod->GetName() : "(none)";
				logger::info("[OAR-Transition] '{}' shouldReplace {}->{} winner='{}' (cands total={} disabled={} evalFalse={} noCond={} clipGen={:X} t={:.3f})",
					resolvedSuffix, prevKnown ? (prev ? "true" : "false") : "?",
					shouldReplace ? "true" : "false", winnerName,
					totalCands, disabledCands, evalFalseCands, noCondCands,
					reinterpret_cast<uintptr_t>(a_this), a_this->GetLocalTime());

				if (!shouldReplace && evalFalseCands > 0) {
					for (auto* info : candidates) {
						if (!info || !info->parentSubMod || info->parentSubMod->IsDisabled()) continue;
						auto* cs = info->parentSubMod->GetConditionSet();
						if (!cs || cs->IsEmpty()) continue;
						logger::info("[OAR-CondDetail]   SubMod='{}' conditions:", info->parentSubMod->GetName());
						for (const auto& cond : cs->GetConditions()) {
							if (!cond) continue;
							std::string prefix = cond->IsNegated() ? "NOT " : "";
							std::string evalStr = cond->lastEvalResult.has_value()
								? (cond->lastEvalResult.value() ? "PASS" : "FAIL") : "?";
							logger::info("[OAR-CondDetail]     {}{} [{}] -> {}",
								prefix, cond->GetName(), cond->GetParameterString(), evalStr);
						}
					}
				}
			}

			// Reset variant state when conditions transition true→false for kWhileActive policy
			if (prevKnown && prev && !shouldReplace) {
				auto* transRefr = GetRefrFromContext(a_context);
				if (!transRefr) transRefr = RE::PlayerCharacter::GetSingleton();
				if (transRefr) {
					for (auto* info : candidates) {
						if (!info || !info->parentSubMod) continue;
						if (info->parentSubMod->variantRerollPolicy == VariantRerollPolicy::kWhileActive) {
							for (auto* ra : info->parentSubMod->GetReplacementAnimations()) {
								if (ra && ra->HasVariants()) {
									ra->GetVariants()->ResetState(transRefr->GetFormID());
								}
							}
						}
					}
				}
			}
			}
		}

		{
			static std::atomic<int> s_updateDiagCounter{ 0 };
			int count = s_updateDiagCounter.fetch_add(1);
			if (count < 5 || count % 3000 == 0) {
				logger::info("[OAR-Diag] Update running for '{}': shouldReplace={} animSlot={:X} original={:X} current={:X}",
					resolvedSuffix, shouldReplace,
					reinterpret_cast<uintptr_t>(animSlot),
					reinterpret_cast<uintptr_t>(originalAnim),
					reinterpret_cast<uintptr_t>(*animSlot));
			}
		}

		if (shouldReplace) {
			// Conditions passed — cancel any pending deactivation delay
			{
				std::unique_lock ddLock(s_deactDelayMutex);
				auto ddIt = s_deactivationDelay.find(a_this);
				if (ddIt != s_deactivationDelay.end()) {
					ddIt->second.active = false;
					ddIt->second.remaining = 0.f;
				}
			}

			auto* cache = AnimationCache::GetSingleton();
			std::string variantSuffix;

			// Variant selection: if the winning replacement has variants and they're enabled, pick one
			if (winningInfo && winningInfo->replacementAnim &&
				winningInfo->replacementAnim->HasVariants() && refr) {
				auto* subMod = winningInfo->parentSubMod;
				if (subMod && subMod->variantsEnabled) {
					if (subMod->variantRerollPolicy == VariantRerollPolicy::kOnEachPlay) {
						// Per-clip caching: each clip generator gets its own fresh roll
						// that is stable for the clip's entire lifetime (no mid-play re-rolls)
						{
							std::shared_lock cvLock(s_clipVariantMutex);
							auto cvIt = s_clipVariantCache.find(a_this);
							if (cvIt != s_clipVariantCache.end()) {
								variantSuffix = cvIt->second;
							}
						}
						if (variantSuffix.empty()) {
							auto* variants = winningInfo->replacementAnim->GetVariants();
							int32_t idx = variants->SelectRandomIndex_Fresh();
							if (idx >= 0 && idx < static_cast<int32_t>(variants->GetCount())) {
								variantSuffix = variants->GetEntries()[idx].cacheSuffix;
							}
							{
								std::unique_lock cvLock(s_clipVariantMutex);
								s_clipVariantCache[a_this] = variantSuffix;
							}
							static std::atomic<int> s_variantSelectLog{ 0 };
							int vCount = s_variantSelectLog.fetch_add(1);
							if (vCount < 30 || vCount % 500 == 0) {
								logger::info("[OAR-Variant] Fresh roll (OnEachPlay) clip={:X} refr={:X}: suffix='{}' (count={})",
									reinterpret_cast<uintptr_t>(a_this), refr->GetFormID(), variantSuffix,
									variants->GetCount());
							}
						}
					} else {
						// kWhileActive: use actor-keyed caching (persists across clips while conditions hold)
						bool shareResults = subMod->GetShareRandomResults();
						variantSuffix = winningInfo->replacementAnim->GetVariants()->SelectVariantSuffix(
							refr->GetFormID(), false, shareResults);
					}
				}
			}

			const std::string& cacheSuffix = variantSuffix.empty() ? resolvedSuffix : variantSuffix;
			// Select the winning SubMod's own file under this suffix — several
			// SubMods can register the same suffix (and variant suffixes can
			// collide across SubMods too), each with a different .hkx.
			const void* winningOwner = winningInfo ? winningInfo->parentSubMod : nullptr;
			// Orphan recovery never rebuilds/swaps (no valid original template,
			// and the clone is already installed) — leave replacement null so
			// the swap block below is skipped; only the annotation/trigger fix runs.
			auto* replacement = orphanAnnotOnly ? nullptr
				: cache->GetOrBuildRuntimeAnim(cacheSuffix, originalAnim, winningOwner);
			if (!replacement && !orphanAnnotOnly) {
				static int s_buildFailLog = 0;
				if (s_buildFailLog < 30) {
					logger::warn("[OAR-BuildFail] GetOrBuildRuntimeAnim('{}') returned null (original={:X} owner={:X})",
						cacheSuffix, reinterpret_cast<uintptr_t>(originalAnim),
						reinterpret_cast<uintptr_t>(winningOwner));
					s_buildFailLog++;
				}
			}

			// Arm only after the replacement clone exists (or orphan recovery
			// proves it is already installed). Any original animation path is
			// eligible when the winning submod has the option enabled.
			if ((replacement || orphanAnnotOnly) && winningInfo && refr) {
				UpdateIdleStopSuppressionArm(a_this, refr, winningInfo);
			}
			bool bReplaceAnnot = winningInfo && winningInfo->parentSubMod ?
				winningInfo->parentSubMod->GetReplaceAnnotations() : true;

			// ---- Partial body (trackFilter) path ----
			// When the winning submod has trackFilter.enabled, do NOT swap the animation
			// slot. Instead register the replacement so Generate can sample it per-bone.
		bool useTrackFilter = winningInfo && winningInfo->parentSubMod &&
			winningInfo->parentSubMod->trackFilter.enabled && replacement;

		if (orphanAnnotOnly) {
			// Clone already in the slot from before the save load. Do NOT rebuild
			// or touch the animation slot. Just make annotation delivery correct:
			// NULL the native triggers (so the original mod file's own annotations
			// stop firing) when we're going to fire ours, and record the active
			// submod so the interruptible/annotation logic downstream works. The
			// manual annotation-firing block further below does the rest.
			if (bReplaceAnnot) {
				bool alreadyRestored = false;
				{
					std::lock_guard rg(s_triggersRestoredMutex);
					alreadyRestored = s_triggersRestoredSet.count(a_this) > 0;
				}
				if (!alreadyRestored) {
					InstallReplacementTriggers(a_this, cacheSuffix);
				}
			}
			if (winningInfo && winningInfo->parentSubMod) {
				std::unique_lock smLock(s_activeSubModMutex);
				s_activeSubModMap[a_this] = winningInfo->parentSubMod;
				s_activeSubModBinding[a_this] = originalAnim;
			}
		} else {

			// Track-filtered clips honor the submod's Replace Annotations setting,
			// same as full-body replacements. The original stays in the slot with
			// intact triggers, so its own annotations always fire natively; when
			// Replace Annotations is ON the donor file's annotations are ALSO
			// fired manually (the donor isn't in the slot, so the engine can't).
			// This used to be forced ON for all track filters — wrong for
			// pose-donor workflows: a filter sampling a frame from a reload
			// animation manually replayed the reload's SoundPlay.* annotations
			// AND reloadComplete, which the game answered by refilling the mag
			// ('Sig Idle Empty', 2026-07-31 session log).
		if (useTrackFilter) {
			{
				std::unique_lock tfLock(s_trackFilterMutex);
				RE::TESObjectREFR* tfActor = refr;
				if (!tfActor) tfActor = RE::PlayerCharacter::GetSingleton();
				if (tfActor) {
					// Per-filter state: each track-filtered submod active on this actor
					// gets its own entry so concurrent filters never evict each other
					// (and each blends in/out with ITS OWN configured times).
					auto* filterKey = &winningInfo->parentSubMod->trackFilter;
					auto* statePtr = FindTrackFilterState(tfActor, filterKey);
					const bool isNew = (statePtr == nullptr);
					if (isNew) {
						statePtr = &s_charTrackFilterMap[tfActor].emplace_back();
						statePtr->filter = filterKey;
					}
					auto& state = *statePtr;
					// NOTE: when the replacement pointer changes (variant re-roll on
					// clip re-activation, clone rebuild), the sample caches are kept
					// — NOT cleared. Clearing them made every non-source clip skip
					// application until the source clip's next Generate, a 1-2 frame
					// dropout that is invisible at high framerate but a visible pop
					// during frame hitches. The stale values are same-filter/same-bone
					// (typically a different variant of the same pose) and get
					// overwritten by the source clip's very next Generate anyway.
					// The donor's OWN track->bone map (from its file's binding),
					// refreshed whenever the served suffix changes. Sampling the
					// donor through the HOST clip's binding is only correct when
					// both share a track layout — true for the donor's own weapon
					// (path matching), false for Leaf Matching claims on other
					// weapons' clips (the MCX glitch, 2026-08-16).
					if (state.replacement != replacement || state.suffix != cacheSuffix ||
						(!state.donorMapQueried)) {
						state.donorTrackToBone.clear();
						state.donorMapIdentity = false;
						state.donorMapQueried = true;
						state.cameraDonorFrameZeroTracks.clear();
						state.invalidCameraReferenceTracks.clear();
						AnimationCache::GetSingleton()->GetDonorTrackMap(
							cacheSuffix, winningInfo->parentSubMod,
							state.donorTrackToBone, state.donorMapIdentity);
					}
					state.replacement = replacement;
					state.parentSubMod = winningInfo->parentSubMod;
					state.standaloneSpecialIdle = false;
					state.sourceClip = a_this;
					state.sourceClips.insert(a_this);
					state.suffix = cacheSuffix;
					state.lastSourceTimeSec = s_tfNowSec.load(std::memory_order_relaxed);
					// Store the original animation pointer for blend-sibling identification.
					// Track filter doesn't swap the animation slot, so the source clip's
					// current animation IS the original.
					if (animSlot && *animSlot) {
						state.sourceAnimation = *animSlot;
					}
					if (isNew) {
						s_trackFilterActiveCount.fetch_add(1, std::memory_order_relaxed);
						// Initialize blend-in state
						state.frozenByName.clear();
						float blendIn = winningInfo->parentSubMod->trackFilter.blendInTime;
						state.blendAlpha = (blendIn <= 0.0f) ? 1.0f : 0.0f;
						state.blendElapsed = 0.0f;
						state.blendDuration = blendIn;
						state.blendingOut = false;
						state.lastSampleSec = state.lastSourceTimeSec;
						state.lastSampledLocalTime = -1.0f;
						state.lastAdvanceSec = state.lastSourceTimeSec;
						// Custom "on start" events fire for track-filtered submods
						// too. The swap path queues these at slot-swap time, but a
						// track filter never swaps the slot, so without this the
						// events were silently skipped (found 2026-08-03: a
						// CullBone.X start event on a track-filtered submod never
						// appeared in the log). Fired only on NEW registration —
						// re-registration of a live filter (including one that is
						// blending out) is a continuation, not a new start.
						QueueCustomEvents(tfActor, winningInfo->parentSubMod->eventsOnStart, "onStart/trackFilter");
					}
					// Re-registration only cancels an in-progress blend-out (or wakes
					// a dormant one-shot) when this clip is at the START of a play.
					// The Update hook re-registers EVERY frame while the generator
					// stays alive, and the old unconditional cancel here fought the
					// tick updater's condition-driven blend-out — the log showed
					// "Blend-out started" restarting 9 times in 150ms mid-throw
					// (2026-08-04 grenade session). A mid-clip re-registration is a
					// continuation and must leave blend state alone.
					if (state.blendingOut || state.dormant) {
						const bool freshPlay = a_this->GetLocalTime() <= 0.15f;
						if (freshPlay) {
							// An armed end-anchored fade counts as ended even before it
							// reaches dormancy: re-throwing during the fade is a new
							// play and must re-fire eventsOnStart.
							const bool wasEnded = state.dormant || state.oneShotDone || state.earlyBlendOutArmed;
							state.blendingOut = false;
							state.dormant = false;
							state.oneShotDone = false;
							state.sampleStarved = false;
							state.frozenByName.clear();
							state.lastSampledLocalTime = -1.0f;
							state.lastSampleSec = s_tfNowSec.load(std::memory_order_relaxed);
							state.lastAdvanceSec = state.lastSampleSec;
							state.selfAdvanceStartSec = -1.0f;
							state.selfAdvanceBaseTime = 0.0f;
							state.earlyBlendOutArmed = false;
							// Blend in from the CURRENT alpha (0 if dormant) rather than
							// snapping, so cancelling a half-done blend-out doesn't pop.
							float blendIn = winningInfo->parentSubMod->trackFilter.blendInTime;
							state.blendDuration = blendIn;
							state.blendElapsed = (blendIn > 0.0f)
	? InverseBlendCurve(CurveOf(state.parentSubMod), state.blendAlpha) * blendIn : 0.0f;
							if (blendIn <= 0.0f) state.blendAlpha = 1.0f;
							if (wasEnded) {
								// The previous play already delivered onEnd; this is a
								// genuinely new play, so onStart fires again.
								state.onEndFired = false;
								QueueCustomEvents(tfActor, winningInfo->parentSubMod->eventsOnStart, "onStart/trackFilter-restart");
							}
						}
					}
				}
			}
			if (*animSlot != originalAnim && originalAnim) {
				*animSlot = originalAnim;
			}
			// Replace Annotations for track filters: the original stays in the
			// slot, so its native triggers fire the ORIGINAL's annotations while
			// the manual pipeline fires the DONOR's — both firing doubled the
			// gameplay events (two grenades thrown per throw, 2026-08-04 session).
			// NULL the source clip's triggers exactly like the swap path does;
			// BuildBehaviorOnlyTriggers keeps graph-critical transition events
			// alive. This runs every Update, doubling as the per-frame re-assert.
			//
			// End Clip If Shorter rides the same install: the built array's
			// relative-to-end transition triggers are re-timed to the DONOR's
			// duration, so the engine ends the clip's state at the donor's end.
			// When Replace Annotations is OFF, the array is still installed for
			// the re-timing but KEEPS the native annotations (nothing else may
			// fire them).
			{
				float endClipDur = EndClipIfShorterDuration(
					winningInfo->parentSubMod, a_this, replacement, originalAnim);
				// Blend-before-end (tickbox off): shift the effective clip end
				// earlier by the blend-out time, so the engine's exit transition
				// overlaps the donor's tail. Before the transition fires there is
				// nothing to blend INTO — the next state's animation only exists
				// once the exit begins (2026-08-14: a 0.5s fade spent 0.3s against
				// an invisible target). With the shift, the incoming state arrives
				// at the top of the fade window: the source-side stamp is pinned
				// (see the Generate hook) while the fading stamp on the incoming
				// clips blends donor -> next state across blendOutTime. Blend Out
				// After End keeps the exit at the donor's true end.
				if (endClipDur > 0.0f && !winningInfo->parentSubMod->trackFilter.blendOutAtEnd) {
					const float bo = winningInfo->parentSubMod->trackFilter.blendOutTime;
					if (bo > 0.0f && endClipDur - bo > 0.2f) {
						endClipDur -= bo;
					}
				}
				if (bReplaceAnnot || endClipDur > 0.0f) {
					bool alreadyRestored = false;
					{
						std::lock_guard rg(s_triggersRestoredMutex);
						alreadyRestored = s_triggersRestoredSet.count(a_this) > 0;
					}
					if (!alreadyRestored) {
						EnsureReplacementTriggersInstalled(a_this, cacheSuffix,
							endClipDur, /*a_keepAnnotations=*/!bReplaceAnnot);
					}
				}
			}
			// Record in activeSubModMap so the interruptible check works for track-filtered submods too
			if (winningInfo->parentSubMod) {
				std::unique_lock smLock(s_activeSubModMutex);
				s_activeSubModMap[a_this] = winningInfo->parentSubMod;
				s_activeSubModBinding[a_this] = originalAnim;
			}
			static int s_tfLog = 0;
			if (s_tfLog < 3) {
				logger::info("[OAR-TrackFilter] Registered filtered replacement for '{}' on actor {:X} (submod '{}')",
					resolvedSuffix, reinterpret_cast<uintptr_t>(refr),
					winningInfo->parentSubMod->GetName());
				s_tfLog++;
			}
		} else if (replacement) {
				// ---- Standard full-body replacement path ----
				auto repVtbl = *reinterpret_cast<uintptr_t*>(replacement);
				if (IsInGameModule(repVtbl)) {
					if (*animSlot != replacement) {
						static int s_swapLog = 0;
						if (s_swapLog < 50) {
							logger::info("[OAR] Swapping clip '{}' -> replacement (conditions passed, clipGen={:X})",
								resolvedSuffix, reinterpret_cast<uintptr_t>(a_this));
							s_swapLog++;
						}
						*animSlot = replacement;

						// Record the un-replace recipe against the SLOT ADDRESS:
						// the binding this slot lives in survives save loads even
						// when every clip dies, and this backup is what lets
						// kPreLoadGame restore the game original into it (see
						// BindingSlotBackup). originalAnim is guaranteed here: the
						// clone was just built from it.
						{
							std::unique_lock bsLock(s_bindingSlotBackupMutex);
							auto& bs = s_bindingSlotBackup[animSlot];
							bs.clone = replacement;
							bs.original = originalAnim;
						}

						// When playOnceFullBody is active, keep original triggers intact so the
						// Havok state machine can still transition out of the current state
						// (e.g. reloadComplete → exit reload). Trigger NULLing blocks internal
						// hkbStateMachine transitions because it only reads from the trigger array.
						//
						// End Clip If Shorter rides the same install (see the TF path):
						// the built array's relative-to-end transition triggers are
						// re-timed to the REPLACEMENT's duration so the state ends at
						// its end. With Replace Annotations off (or playOnce keeping
						// the originals), the array is still installed for the
						// re-timing but keeps the native annotations.
						bool skipTriggerNull = (winningInfo && winningInfo->parentSubMod &&
							winningInfo->parentSubMod->GetPlayOnceFullBody());
						const float endClipDurSwap = EndClipIfShorterDuration(
							winningInfo ? winningInfo->parentSubMod : nullptr, a_this, replacement, originalAnim);
						if (bReplaceAnnot && !skipTriggerNull) {
							InstallReplacementTriggers(a_this, cacheSuffix, endClipDurSwap);
						} else if (endClipDurSwap > 0.0f) {
							InstallReplacementTriggers(a_this, cacheSuffix, endClipDurSwap,
								/*a_keepAnnotations=*/true);
						}

						// Fire custom "on start" events and track active SubMod
						if (winningInfo && winningInfo->parentSubMod && refr) {
							QueueCustomEvents(refr, winningInfo->parentSubMod->eventsOnStart, "onStart");
							std::unique_lock smLock(s_activeSubModMutex);
							s_activeSubModMap[a_this] = winningInfo->parentSubMod;
							s_activeSubModBinding[a_this] = originalAnim;
						}

						if (AnimationLog::GetSingleton()->IsEnabled() && winningInfo) {
							std::string subModName = winningInfo->parentSubMod ?
								winningInfo->parentSubMod->GetName() : "";
							std::string realPathCopy;
							{
								std::shared_lock plock(s_clipRealPathMutex);
								auto pit = s_clipRealPathCache.find(a_this);
								if (pit != s_clipRealPathCache.end()) realPathCopy = pit->second;
							}
							AnimationLog::GetSingleton()->AddEntry(
								AnimationLog::EventType::kReplace,
								ResolveLogRefr(a_this, a_context), resolvedSuffix,
								winningInfo->replacementPath, subModName,
								realPathCopy, ClassifyClipPerspective(a_this, realPathCopy));
						}
					}

					// Start full-body blend-in if SubMod has a blend time configured.
					// Also register when only a blend-OUT time is set (blend-in 0):
					// the driver snaps alpha to 1 instantly for a zero blend-in, but
					// the map entry must exist for the blend-out to run later.
					float blendTime = (winningInfo && winningInfo->parentSubMod)
						? winningInfo->parentSubMod->GetCustomBlendTimeOnInterrupt() : -1.0f;
					if (blendTime < 0.0f) blendTime = 0.0f;
					float blendOutCfg = (winningInfo && winningInfo->parentSubMod)
						? winningInfo->parentSubMod->GetCustomBlendOutTime() : -1.0f;
					if (originalAnim) {
						RE::TESObjectREFR* blendActor = refr ? refr : RE::PlayerCharacter::GetSingleton();
						if (blendActor) {
							ActorClipKey key{ blendActor, suffix };
							std::unique_lock fbLock(s_fullBodyBlendMutex);
							auto exIt = s_fullBodyBlendMap.find(key);
							bool isNew = (exIt == s_fullBodyBlendMap.end());
							// Create an entry only when this submod configures a blend —
							// but if one already EXISTS, process it even when the new
							// submod has no blend times: a replacement→replacement
							// switch must still honor the OUTGOING submod's blend-out.
							if (blendTime > 0.0f || blendOutCfg > 0.0f || !isNew) {
							auto& bs = isNew ? s_fullBodyBlendMap[key] : exIt->second;
							if (isNew || bs.replacement != replacement) {
								// Default: blend from the vanilla original (fresh
								// replacement start). On a replacement→replacement
								// switch (another submod won mid-clip), blend from the
								// OUTGOING replacement's pose instead, over at least
								// the outgoing submod's effective blend-out time — the
								// game never returns to the original in between, so
								// this is where its blend-out applies.
								RE::hkaAnimation* blendFrom = originalAnim;
								bool isSwitch = (!isNew && bs.replacement && bs.replacement != replacement);
								if (isSwitch) {
									blendFrom = bs.replacement;
									// Outgoing effective blend-out: its configured out
									// time, or (mirror default) its current blendDuration
									// (the in time while blending in / steady, or the out
									// time if it was already blending out).
									float outgoingOut = (bs.blendOutDuration >= 0.0f)
										? bs.blendOutDuration : bs.blendDuration;
									if (outgoingOut > blendTime) blendTime = outgoingOut;
								}
								bs.replacement = replacement;
								bs.original = blendFrom;
								bs.ownerClip = a_this;
								bs.blendElapsed = 0.0f;
								// Zero blend-in (entry exists only for its blend-out):
								// start settled at alpha 1 so Generate never outputs a
								// frame of the original pose while "blending in".
								bs.blendAlpha = (blendTime > 0.0f) ? 0.0f : 1.0f;
								bs.blendingIn = (blendTime > 0.0f);
								bs.blendingOut = false;
								bs.blendDuration = blendTime;
								bs.blendOutDuration = blendOutCfg;
								bs.blendCurve = CurveOf(winningInfo ? winningInfo->parentSubMod : nullptr);
								bs.poseSnapshotValid = false;
								bs.poseSnapshot.clear();
								if (isNew) s_fullBodyBlendActiveCount.fetch_add(1, std::memory_order_relaxed);
								static int s_fbRegLog = 0;
								if (s_fbRegLog < 10) {
									logger::info("[OAR-FullBodyBlend] Registered blend-in: suffix='{}' dur={:.2f}s switch={}",
										suffix, blendTime, isSwitch);
									s_fbRegLog++;
								}
							} else if (bs.blendingOut) {
								// Re-activation during blend-out: cancel, resume blend-in.
								// blendDuration currently holds the blend-OUT time (set
								// when the blend-out started) — restore the in time.
								bs.blendingOut = false;
								bs.blendingIn = (blendTime > 0.0f);
								if (!bs.blendingIn) bs.blendAlpha = 1.0f;
								bs.blendElapsed = 0.0f;
								bs.blendDuration = blendTime;
								bs.poseSnapshotValid = false;
								bs.ownerClip = a_this;
							}
							}
						}
					}
				{
					// Per-frame re-assert. End Clip If Shorter needs the install even
					// when Replace Annotations is off — with native annotations kept
					// (see the swap-site comment). Only when NEITHER applies do the
					// original triggers get restored.
					const float endClipDurKeep = EndClipIfShorterDuration(
						winningInfo ? winningInfo->parentSubMod : nullptr, a_this, replacement, originalAnim);
					if (bReplaceAnnot || endClipDurKeep > 0.0f) {
						bool alreadyRestored = false;
						{
							std::lock_guard rg(s_triggersRestoredMutex);
							alreadyRestored = s_triggersRestoredSet.count(a_this) > 0;
						}
						if (!alreadyRestored) {
							EnsureReplacementTriggersInstalled(a_this, cacheSuffix,
								endClipDurKeep, /*a_keepAnnotations=*/!bReplaceAnnot);
						}
					} else {
						RestoreClipTriggers(a_this);
					}
				}
				}
			}
		}

		ActiveReplacementEntry entry;
		entry.clipSuffix = resolvedSuffix;
		// Attach the full resolved on-disk path (when Source S resolved it) so
		// the Active Replacements window can display the real animation path.
		{
			std::shared_lock plock(s_clipRealPathMutex);
			auto pit = s_clipRealPathCache.find(a_this);
			if (pit != s_clipRealPathCache.end()) entry.fullPath = pit->second;
		}
		entry.conditionsPassed = true;
		if (winningInfo) {
			if (!variantSuffix.empty()) {
				entry.replacementPath = variantSuffix + " (variant)";
			} else {
				entry.replacementPath = winningInfo->replacementPath;
			}
			if (winningInfo->parentSubMod) {
				entry.subModName = winningInfo->parentSubMod->GetName();
				entry.subMod = winningInfo->parentSubMod;
			}
		}
		uint32_t actorID = 0;
		if (refr) {
			actorID = refr->GetFormID();
			entry.actorFormID = actorID;
			auto name = RE::TESFullName::GetFullName(*refr);
			if (!name.empty())
				entry.actorName = std::string(name);
			else if (actorID == 0x14)
				entry.actorName = "Player";
		}
		ActiveReplacementTracker::GetSingleton()->Update(actorID, resolvedSuffix, entry);

		// Register this suffix as having an active replacement (for event suppression)
		{
			std::unique_lock slock(s_activeReplacementSuffixMutex);
			s_activeReplacementSuffixes.insert(resolvedSuffix);
		}
		// Track per-actor active replacements
		{
			std::unique_lock alock(s_activeReplacementActorMutex);
			s_activeReplacementByActor[actorID].insert(resolvedSuffix);
		}

		// Fire replacement annotations manually with dual-path emission.
		// Phase 1: NotifyAnimationGraphImpl (behavior graph state transitions, bone cull)
		// Phase 2: BSTEventSource::Notify (audio SoundPlay.*, plugin sinks)
		// Only fires when ReplaceAnnotations is enabled for the submod (or forced for track filters).
		//
		// ============================ FUTURE REDESIGN NOTE ============================
		// This whole manual-firing system (localTime tracker, catch-up, seek heuristic,
		// nested-notify suppression, reentrancy depth guard) exists to REPLAY the
		// replacement file's annotations by hand — and every guard below patches a way
		// manual replay diverges from native behavior (ghost activations, time
		// teleports, nested notifies; see crash-2026-07-23-00-04-17 and the phantom
		// equipfast SoundPlay bug).
		//
		// The structurally better design, if annotation edge cases keep appearing
		// (missing sounds during frame hitches, echo-restart timing oddities):
		// LET THE ENGINE FIRE THEM. We already install a hand-built filtered
		// hkbClipTriggerArray (see BuildBehaviorOnlyTriggers — keeps behavior triggers,
		// drops annotation triggers). Extend it to also APPEND trigger entries built
		// from the replacement file's annotations: correct localTimes, reusing the
		// event IDs + hkbStringEventPayload layout of the original's annotation
		// triggers (the [OAR-TrigFilter] dump proves they're readable — e.g. sound
		// annotations are id=74 with a string payload). The engine then fires the
		// replacement's annotations through its native trigger path, which already
		// handles everything the guards below approximate: ghosts don't fire, loops/
		// echoes process at the right point in the update cycle, and there's no
		// reentrancy or localTime bookkeeping at all. Manual firing would remain only
		// for what triggers can't do: PlaySoundDirect fallback, suppressAnnotations
		// filtering (apply at build time instead), and ReloadEnd side-effects.
		// Kept as-is for now because this battle-tested path also feeds dry-fire,
		// suppression, and track-filter annotations (when ReplaceAnnotations is
		// enabled on the submod) — migrate deliberately.
		// ==============================================================================
		if (bReplaceAnnot) {
			const auto& annotSuffix = cacheSuffix;
			// Same owner selection as the animation itself — the fired
			// annotations must come from the file that is actually playing.
			auto* annotations = AnimationCache::GetSingleton()->GetAnnotations(annotSuffix, winningOwner);
			if (annotations && !annotations->empty() && refr) {
				float localTime = a_this->GetLocalTime();
				std::vector<std::string> toFire;

				// Ghost activations: during an outer NotifyAnimationGraphImpl the
				// state machine briefly Updates clips that aren't really playing
				// (e.g. wpnequipfast while melee resolves). Their localTime often
				// jumps 0 → end in one frame; catch-up then dumps every SoundPlay.
				// Nested updates are fully PASSIVE here: no firing (synchronous
				// dispatch mid-graph-update is unsafe/dropped) and no seeking.
				// Seeking forward while nested marked annotations between two real
				// Updates as already fired — the graph is busiest with notifies
				// right at animation start (equip/reload events, our own annotation
				// dispatch), which is why early CullBone/UncullBone annotations
				// were being swallowed. The next REAL Update fires the whole
				// [prev, cur] window through the normal path instead.
				const bool nestedNotify = (s_notifyAnimGraphDepth > 0);
				// Max gap we'll still treat as continuous playback. Generous on
				// purpose: equip/reload starts routinely hitch 100-300ms while
				// assets load, and a smaller cap (was 0.12) turned every hitch
				// into a silent seek that dropped early annotations. Jumps larger
				// than this = seek without firing sounds (time teleport / ghost
				// complete), but bone-visibility annotations are still replayed —
				// see replayBoneVisAnnots below.
				constexpr float kMaxAnnotCatchUpSec = 0.30f;

				auto seekIndexToTime = [&](float t) -> int32_t {
					int32_t idx = -1;
					for (int32_t i = 0; i < static_cast<int32_t>(annotations->size()); ++i) {
						if ((*annotations)[i].time <= t) idx = i;
						else break;
					}
					return idx;
				};

				// CullBone./UncullBone. annotations are STATE, not one-shot events:
				// dropping one desyncs bone visibility for the rest of the session
				// (a culled magazine stays invisible until something unculls it).
				auto isBoneVisAnnot = [](const std::string& t) -> bool {
					return _strnicmp(t.c_str(), "CullBone.", 9) == 0 ||
					       _strnicmp(t.c_str(), "UncullBone.", 11) == 0;
				};

				// When a seek skips annotations without firing them, the skipped
				// bone-visibility annotations are still replayed in order so the
				// authored cull state at the destination time is reached. They are
				// idempotent per bone, so replaying is always safe; sounds and
				// graph events stay dropped (firing a 2-second-old SoundPlay dump
				// is exactly what the seek exists to prevent).
				auto replayBoneVisAnnots = [&](int32_t fromIdx, int32_t toIdx,
					std::vector<std::string>& out) {
					for (int32_t i = fromIdx; i <= toIdx &&
						i < static_cast<int32_t>(annotations->size()); ++i) {
						if (i < 0) continue;
						if (isBoneVisAnnot((*annotations)[i].text)) {
							out.push_back((*annotations)[i].text);
						}
					}
				};

				{
					std::unique_lock alock(s_annotStateMutex);
					auto& astate = s_annotStateMap[a_this];

					if (astate.activeSuffix != annotSuffix || astate.activeOwner != winningOwner) {
						astate.activeSuffix = annotSuffix;
						astate.activeOwner = winningOwner;
						static int s_annotInitLog = 0;
						if (s_annotInitLog < 30) {
							logger::info("[OAR-Annot] Init tracking for '{}' ({} annotations, localTime={:.3f}, nested={})",
								annotSuffix, annotations->size(), localTime, nestedNotify);
							s_annotInitLog++;
						}
						// Always start the window pinned at 0 — even when nested, and
						// even when localTime is already past the start. Replacement
						// swaps install during the activation frame's NESTED graph
						// update; seeking here marked the t=0 annotations as already
						// fired, and bone-visibility STATE SETUP lives at t=0 (P890
						// reload: UncullBone.WeaponExtra3 / CullBone.
						// Weaponmagazinechild3) — swallowing it left the spare
						// magazine mesh permanently hidden. The first REAL Update
						// decides what happens: a small gap fires everything in
						// [0, curT] normally; a genuine late join / ghost jump hits
						// the dt guard below, which seeks but still replays the
						// skipped bone-visibility annotations.
						astate.prevLocalTime = 0.f;
						astate.lastFiredIndex = -1;
					}

					if (localTime >= 0.f && !nestedNotify) {
						float prevT = astate.prevLocalTime;
						float curT = localTime;

						bool looped = (curT < prevT - 0.01f);
						if (looped) {
							if (Settings::GetSingleton()->bLogLoop && AnimationLog::GetSingleton()->IsEnabled()) {
								std::string loopPath;
								{
									std::shared_lock plock(s_clipRealPathMutex);
									auto pit = s_clipRealPathCache.find(a_this);
									if (pit != s_clipRealPathCache.end()) loopPath = pit->second;
								}
								AnimationLog::GetSingleton()->AddEntry(
									AnimationLog::EventType::kLoop,
									ResolveLogRefr(a_this, a_context), suffix, annotSuffix, "",
									loopPath, ClassifyClipPerspective(a_this, loopPath));
							}
							// Loop wrap: only fire the remaining tail if the gap is
							// small; a huge wrap after a teleport is a seek, not a
							// play — but bone-visibility state in the tail must still
							// land (see replayBoneVisAnnots).
							if ((astate.prevLocalTime - curT) <= kMaxAnnotCatchUpSec) {
								for (int32_t i = astate.lastFiredIndex + 1; i < static_cast<int32_t>(annotations->size()); ++i) {
									auto& ann = (*annotations)[i];
									if (ann.time >= prevT) {
										toFire.push_back(ann.text);
									}
								}
							} else {
								replayBoneVisAnnots(astate.lastFiredIndex + 1,
									static_cast<int32_t>(annotations->size()) - 1, toFire);
							}
							astate.lastFiredIndex = -1;
							prevT = 0.f;

							// Signal that a loop occurred — non-interruptible submods with
							// replaceOnLoop=true will re-evaluate conditions once.
							{
								std::unique_lock leLock(s_loopEchoFlagMutex);
								s_clipLoopPending[a_this] = true;
							}

							// If keepRandomResultsOnLoop is false, clear the per-clip
							// variant cache so a new variant is selected on loop.
							{
								std::shared_lock smLock(s_activeSubModMutex);
								auto smIt = s_activeSubModMap.find(a_this);
								if (smIt != s_activeSubModMap.end() && smIt->second &&
									!smIt->second->GetKeepRandomResultsOnLoop()) {
									std::unique_lock cvLock(s_clipVariantMutex);
									s_clipVariantCache.erase(a_this);
								}
							}
						}

						const float dt = curT - prevT;
						if (dt > kMaxAnnotCatchUpSec) {
							// Time jumped (ghost complete / scrub / late join) — seek
							// without dumping sounds, but replay the skipped
							// bone-visibility annotations so cull state stays correct.
							const int32_t seekIdx = seekIndexToTime(curT);
							replayBoneVisAnnots(astate.lastFiredIndex + 1, seekIdx, toFire);
							static int s_seekLog = 0;
							if (s_seekLog < 30) {
								logger::info("[OAR-Annot] Seek '{}' prev={:.3f} cur={:.3f} (dt={:.3f}, replaying {} bone-vis annots)",
									annotSuffix, prevT, curT, dt, toFire.size());
								s_seekLog++;
							}
							astate.lastFiredIndex = seekIdx;
						} else {
							for (int32_t i = astate.lastFiredIndex + 1; i < static_cast<int32_t>(annotations->size()); ++i) {
								auto& ann = (*annotations)[i];
								if (ann.time > curT) break;
								if (ann.time >= prevT) {
									toFire.push_back(ann.text);
									astate.lastFiredIndex = i;
								}
							}
						}
					} else if (nestedNotify) {
						// Nested update: fully passive. No firing (unsafe mid-graph-
						// update) and no seeking — advancing lastFiredIndex here
						// swallowed annotations that a real Update between notifies
						// had not fired yet. The tracker keeps its last real-Update
						// position; the next real Update fires (small gap) or seeks
						// with bone-vis replay (big gap). Ghosts that only ever see
						// nested updates therefore fire nothing, same as before.
						static int s_nestSkipLog = 0;
						if (s_nestSkipLog < 30) {
							logger::info("[OAR-Annot] Nested Update — passive for '{}' at t={:.3f} (tracker at {:.3f})",
								annotSuffix, localTime, astate.prevLocalTime);
							s_nestSkipLog++;
						}
					}
					// Only real Updates advance the window; nested updates must not
					// move prevLocalTime past annotations they didn't fire.
					if (!nestedNotify && localTime >= 0.f) {
						astate.prevLocalTime = localTime;
					}
				}
				// Lock released — drop suppressed annotations before firing.
				// "suppressAnnotations" in the winning SubMod's config mutes
				// specific annotation texts (or all of them) — e.g. muting
				// "WeaponFire" for a dry-fire replacement whose source file
				// still carries the fire annotation.
				if (!toFire.empty() && winningInfo && winningInfo->parentSubMod) {
					auto* annotSubMod = winningInfo->parentSubMod;
					std::erase_if(toFire, [&](const std::string& t) {
						if (annotSubMod->IsAnnotationSuppressed(t)) {
							static int s_suppressLog = 0;
							if (s_suppressLog < 30) {
								logger::info("[OAR-Annot] Suppressed '{}' (submod '{}')",
									t, annotSubMod->GetName());
								s_suppressLog++;
							}
							return true;
						}
						return false;
					});
				}
				if (!toFire.empty()) {
					// Also skip graph notify if this clip has no animation control
					// yet — Activate can call Update before the control exists.
					const bool ctrlReady = (a_this->GetAnimationControlRaw() != nullptr);

					s_oarFiringAnnotations = true;
					for (auto& text : toFire) {
						static constexpr const char* kSoundPlayPrefix = "SoundPlay.";
						static constexpr size_t kSoundPlayLen = 10;

						if (text.size() > kSoundPlayLen &&
							_strnicmp(text.c_str(), kSoundPlayPrefix, kSoundPlayLen) == 0)
						{
							// SoundPlay: direct audio — never touches the behavior graph.
							const char* soundName = text.c_str() + kSoundPlayLen;
							PlaySoundDirect(soundName, refr);
						} else {
							// Graph + sink notify, synchronous (correct clip timing).
							// SEH in SafeNotify catches residual AVs.
							RE::BSFixedString evtName(text.c_str());
							if (ctrlReady) {
								SafeNotifyAnimGraph(refr, evtName);
							}
							SafeNotifyEventSinks(refr, evtName);

							if (text == "ReloadEnd" || text == "reloadEnd") {
								auto* actor = refr->As<RE::Actor>();
								if (actor) {
									SetHavokBool(actor, kHavokVar_IsReloading, false);
								}
								RestoreClipTriggers(a_this);
								{
									std::lock_guard rg(s_triggersRestoredMutex);
									s_triggersRestoredSet.insert(a_this);
								}
								{
									std::unique_lock poLock(s_playOnceDecisionMutex);
									s_playOnceDecision.erase(a_this);
								}
							}
						}

						// High cap on purpose (was 50, exhausted mid-session and made
						// later plays look like they lost annotations in the log).
						static int s_annotFireLog = 0;
						if (s_annotFireLog < 1000) {
							logger::info("[OAR-Annot] Fired '{}' (clip '{}')",
								text, suffix);
							s_annotFireLog++;
						}
					}
					s_oarFiringAnnotations = false;
				}
			}
		}

		// When triggers are NULLed, the behavior graph state machine loses its
		// transition signal (e.g. reloadComplete). Once the replacement animation
		// has completed at least one full playthrough, restore the original triggers
		// so the state machine can transition out of the current state.
		{
			std::shared_lock tLock(s_triggersBackupMutex);
			auto tIt = s_triggersBackup.find(a_this);
			if (tIt != s_triggersBackup.end() && tIt->second.nulled) {
				float localTime = a_this->GetLocalTime();
				auto* curAnim = *animSlot;
				float duration = 0.0f;
				if (curAnim && !IsBadReadPtr(curAnim, sizeof(uintptr_t))) {
					duration = *reinterpret_cast<float*>(
						reinterpret_cast<uint8_t*>(curAnim) + 0x14);
				}
				// NEVER restore on completion for LOOPING clips (auto-fire, idles).
				// The engine processes the t=0 wrap inside _Update with whatever
				// trigger array is installed at that moment — restoring here hands
				// the ORIGINAL animation's t~0 annotations (WeaponFire on fire
				// clips!) back to the engine, which then fires natively on EVERY
				// loop pass, e.g. a dry-fire replacement kept shooting the SCAR.
				// Looping states exit via game action events (attackStop on
				// trigger release), not via their own clip triggers, so they
				// don't need this restore; the uninstall/deactivate paths restore
				// triggers when the replacement actually ends.
				const bool isLoopingClip = (a_this->mode == RE::MODE_LOOPING);
				if (!isLoopingClip && duration > 0.01f && localTime >= duration - 0.01f) {
					tLock.unlock();
					RestoreClipTriggers(a_this);
					{
						std::lock_guard rg(s_triggersRestoredMutex);
						s_triggersRestoredSet.insert(a_this);
					}
					// Clear the playOnceFullBody decision lock so condition
					// re-evaluation resumes and can detect conditions are now false.
					{
						std::unique_lock poLock(s_playOnceDecisionMutex);
						s_playOnceDecision.erase(a_this);
					}
					static int s_trigRestoreLog = 0;
					if (s_trigRestoreLog < 20) {
						logger::info("[OAR-Triggers] Restored triggers for '{}' (anim completed, localTime={:.3f} duration={:.3f} clipGen={:X}) [playOnce unlocked]",
							resolvedSuffix, localTime, duration, reinterpret_cast<uintptr_t>(a_this));
						s_trigRestoreLog++;
					}
				}
			}
		}

	} else {
		// Deactivation delay: if the active submod has a delay configured,
		// hold the replacement in place for that duration after conditions fail.
		{
			std::shared_lock smLock(s_activeSubModMutex);
			auto smIt = s_activeSubModMap.find(a_this);
			if (smIt != s_activeSubModMap.end() && smIt->second) {
				float delay = smIt->second->GetDeactivationDelay();
				if (delay > 0.0f) {
					std::unique_lock ddLock(s_deactDelayMutex);
					auto& ds = s_deactivationDelay[a_this];
					if (!ds.active) {
						// Start the delay timer
						ds.active = true;
						ds.remaining = delay;
						return;
					}
					// Timer is running — decrement and check
					ds.remaining -= a_timestep;
					if (ds.remaining > 0.0f) {
						return;  // Still within delay, keep replacement
					}
					// Timer expired — fall through to normal restore logic
					ds.active = false;
				}
			}
		}

		// NOTE: Do NOT directly erase track filter entries here. The blend-out
		// logic in HookedActorUpdate handles condition-false deactivation with
		// a smooth blend. Erasing here would race with and defeat that blend,
		// causing an instant snap instead of a smooth transition.

		// Conditions failed — check if we should blend out or instant-restore.
		if (originalAnim && *animSlot != originalAnim) {
			// Check if there's a full-body blend entry that needs blend-out
			RE::TESObjectREFR* blendActor = refr ? refr : RE::PlayerCharacter::GetSingleton();
			bool startedBlendOut = false;
			if (blendActor) {
				ActorClipKey key{ blendActor, suffix };
				std::unique_lock fbLock(s_fullBodyBlendMutex);
				auto it = s_fullBodyBlendMap.find(key);
				if (it != s_fullBodyBlendMap.end()) {
					if (it->second.blendingIn) {
						// Another clip with this suffix has conditions=true and is
						// blending in. Don't interfere — skip this clip's deactivation.
						startedBlendOut = true;
					} else if (it->second.blendingOut) {
						startedBlendOut = true;
					} else {
						// Steady state — start blend-out.
						// Swap slot to original NOW so Generate outputs original.
						// The snapshot (captured on first Generate frame) will be replacement.
						// A configured blend-out time (>= 0) wins; negative means
						// mirror the blend-in duration (the original behavior).
						float blendOutTime = 0.0f;
						if (it->second.blendOutDuration >= 0.0f) {
							blendOutTime = it->second.blendOutDuration;
						} else if (it->second.blendDuration > 0.0f) {
							blendOutTime = it->second.blendDuration;
						}
						if (blendOutTime > 0.0f) {
							it->second.blendingOut = true;
							it->second.blendingIn = false;
							it->second.blendElapsed = 0.0f;
							it->second.blendDuration = blendOutTime;
							it->second.original = originalAnim;
							it->second.poseSnapshotValid = false;
							it->second.ownerClip = a_this;
							startedBlendOut = true;
							// Restore slot to original so Generate samples original pose
							*animSlot = originalAnim;
							static int s_fbBoLog = 0;
							if (s_fbBoLog < 10) {
								logger::info("[OAR] Full-body blend-out started for '{}' (duration={:.2f}s)",
									suffix, blendOutTime);
								s_fbBoLog++;
							}
						} else {
							// Explicit blend-out of 0 (instant): take the immediate
							// restore path below, and erase the blend entry now so
							// it doesn't linger with stale clip/animation pointers.
							s_fullBodyBlendMap.erase(it);
							s_fullBodyBlendActiveCount.fetch_sub(1, std::memory_order_relaxed);
						}
					}
				}
			}

			if (!startedBlendOut) {
				if (!IsBadReadPtr(originalAnim, sizeof(uintptr_t))) {
					auto vtbl = *reinterpret_cast<uintptr_t*>(originalAnim);
					if (IsPlausibleGameAnimVtable(vtbl)) {
						// Flush end-of-clip annotations BEFORE restoring the slot:
						// plays that end through this path (conditions flip false
						// after the graph parked the clip near its end) never reach
						// Deactivate promptly, and their tail annotations
						// (reloadEnd, final foley) were lost. Must run while the
						// clone is still in the slot (duration read) and while
						// s_activeSubModMap still has the owner (suppression).
						FlushPendingEndAnnotations(a_this, refr, "condition-fail restore");

						// Fire custom "on end" events before restoring
						{
							std::shared_lock smLock(s_activeSubModMutex);
							auto smIt = s_activeSubModMap.find(a_this);
							if (smIt != s_activeSubModMap.end() && smIt->second && refr) {
								QueueCustomEvents(refr, smIt->second->eventsOnEnd, "onEnd");
							}
						}
						{
							std::unique_lock smLock(s_activeSubModMutex);
							s_activeSubModMap.erase(a_this);
							s_activeSubModBinding.erase(a_this);
						}
						{
							std::lock_guard rg(s_triggersRestoredMutex);
							s_triggersRestoredSet.erase(a_this);
						}

						static int s_restoreLog = 0;
						if (s_restoreLog < 50) {
							logger::info("[OAR] Restoring original for clip '{}' (conditions failed/disabled, clipGen={:X}, t={:.3f})",
								suffix, reinterpret_cast<uintptr_t>(a_this), a_this->GetLocalTime());
							s_restoreLog++;
						}
						*animSlot = originalAnim;
					} else {
						static int s_staleRestoreLog = 0;
						if (s_staleRestoreLog < 20) {
							logger::warn("[OAR] Original stale at restore for '{}' — leaving clone in slot (safe)", suffix);
							s_staleRestoreLog++;
						}
					}
				} else {
					static int s_ibrRestoreLog = 0;
					if (s_ibrRestoreLog < 20) {
						logger::warn("[OAR] Original unreadable at restore for '{}' — leaving clone in slot", suffix);
						s_ibrRestoreLog++;
					}
				}
				// Remove blend entry if present (instant removal) — skip if blending in
				if (blendActor) {
					ActorClipKey key{ blendActor, suffix };
					std::unique_lock fbLock(s_fullBodyBlendMutex);
					auto it = s_fullBodyBlendMap.find(key);
					if (it != s_fullBodyBlendMap.end() && !it->second.blendingIn) {
						s_fullBodyBlendMap.erase(it);
						s_fullBodyBlendActiveCount.fetch_sub(1, std::memory_order_relaxed);
					}
				}
			}
		}
		// Restore the engine's trigger arrays so the original animation's annotations
		// resume firing natively.
		RestoreClipTriggers(a_this);

		// Vanilla annotation backup: the play now runs un-replaced, but its
		// engine-side trigger data may have been built against a stale clone at
		// _Activate (see the VanillaAnnotBackup block). Diff the original's
		// annotations against whatever the clip's live array holds AFTER the
		// trigger restore above, and arm manual firing for any that are
		// missing. Healthy plays arm nothing.
		if (s_gameFullyLoaded.load()) {
			if (auto** vbSlot = a_this->GetAnimationSlot(); vbSlot && *vbSlot &&
				!AnimationCache::GetSingleton()->IsOurReplacement(*vbSlot)) {
				ArmVanillaAnnotationBackup(a_this, *vbSlot, a_this->GetLocalTime());
			}
		}

		uint32_t actorID = refr ? refr->GetFormID() : 0;
		ActiveReplacementTracker::GetSingleton()->Remove(actorID, resolvedSuffix);

		// Clear annotation state and active suffix tracking
		{
			std::unique_lock alock(s_annotStateMutex);
			s_annotStateMap.erase(a_this);
		}
		{
			std::unique_lock slock(s_activeReplacementSuffixMutex);
			s_activeReplacementSuffixes.erase(resolvedSuffix);
		}
		{
			std::unique_lock alock(s_activeReplacementActorMutex);
			auto it = s_activeReplacementByActor.find(actorID);
			if (it != s_activeReplacementByActor.end()) {
				it->second.erase(resolvedSuffix);
				if (it->second.empty()) {
					s_activeReplacementByActor.erase(it);
				}
			}
		}
	}
	}

	void hkbClipGenerator_Deactivate(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context)
	{
		if (a_this) {
			ReleaseIdleStopSuppressionClip(a_this);
			// Flush a still-pending kActivate log entry BEFORE dropping the
			// per-clip state below. Short-lived clips (fire animations,
			// transition clips) often deactivate before the Update-hook grace
			// period elapses — silently erasing the pending entry made them
			// vanish from the Animation Log entirely.
			{
				std::string pendingSuffix;
				{
					std::shared_lock slock(s_clipRealPathStateMutex);
					auto pit = s_pendingActivateLog.find(a_this);
					if (pit != s_pendingActivateLog.end()) {
						pendingSuffix = pit->second.suffix;
					}
				}
				if (!pendingSuffix.empty() && AnimationLog::GetSingleton()->IsEnabled()) {
					// Leaf-validated against the entry's own animation — the
					// cache may hold a path for a different animation by now.
					const auto displayPath = DisplayPathForEntry(a_this, pendingSuffix);
					AnimationLog::GetSingleton()->AddEntry(
						AnimationLog::EventType::kActivate,
						ResolveLogRefr(a_this, a_context),
						pendingSuffix, "", "",
						displayPath,
						ClassifyClipPerspective(a_this, displayPath));
				}
			}

			// End-of-clip annotation flush — see FlushPendingEndAnnotations. Runs
			// before the state erasures below (needs the annot state and the
			// active-submod entry) and before the engine frees the clip (needs
			// the clone still in the animation slot for the duration read).
			{
				auto* deactRefr = GetRefrFromContext(a_context);
				FlushPendingEndAnnotations(a_this, deactRefr, "deactivate");
				// Vanilla backup: fire whatever the un-replaced play still owed
				// when the graph tore it down near its end (same end-window rule),
				// and always drop the entry — the clip is going away.
				FlushVanillaAnnotBackup(a_this, deactRefr, "deactivate");
			}
			{
				std::lock_guard icLock(s_annotIntegrityMutex);
				s_annotIntegrityLastT.erase(a_this);
			}

			// Restore the ORIGINAL into the shared animation binding before this
			// clip goes away. The binding belongs to the CHARACTER's binding set
			// and outlives the clip generator: a clone left here is inherited by
			// the NEXT generator that activates on the same binding. A FRESH
			// generator has no animation control at Activate-hook entry (the
			// control is built inside _Activate from this binding), so BOTH
			// Activate-side restores are structurally blind to it — the new
			// control captures the CLONE's duration and the state's
			// relative-to-end exit then cuts every native annotation past the
			// clone's end on an un-replaced play (MP7 empty reload dropping
			// 05_Bolt/06_Shoulder through a fresh clip generator, 2026-08-18;
			// same family as the Activate catch-all, which still covers RECYCLED
			// generators whose control survives). Restoring the slot AFTER
			// _Activate provably does not help — the Update-hook restore ran on
			// the first frame of that play and the tail was still cut — so the
			// binding must already be clean when _Activate runs.
			// Cache-based lookup (not the per-clip backups erased below), with
			// the same pointer/vtable validation as the Activate catch-all:
			// during graph teardown (weapon switch) the recorded original may
			// already be freed, and then the binding is being torn down too, so
			// leaving the clone is harmless.
			// Runs after the annotation flush above, which needs the clone still
			// in the slot for its duration read.
			if (s_gameFullyLoaded.load()) {
				if (auto** deactSlot = a_this->GetAnimationSlot(); deactSlot && *deactSlot) {
					auto* deactCache = AnimationCache::GetSingleton();
					if (deactCache->IsOurReplacement(*deactSlot)) {
						auto* deactOrig = deactCache->GetOriginalFromReplacement(*deactSlot);
						if (deactOrig && !IsBadReadPtr(deactOrig, sizeof(uintptr_t)) &&
							IsPlausibleGameAnimVtable(*reinterpret_cast<uintptr_t*>(deactOrig))) {
							*deactSlot = deactOrig;
							static std::atomic<int> s_deactRestoreLog{ 0 };
							if (s_deactRestoreLog.fetch_add(1, std::memory_order_relaxed) < 30) {
								logger::info("[OAR] Deactivate: restored original into shared binding (clipGen={:X})",
									reinterpret_cast<uintptr_t>(a_this));
							}
						}
					}
				}
			}

			// Do NOT restore triggers or touch the per-clip backups during
			// deactivation beyond the validated binding restore above. The clip
			// is being freed and ALL backed-up pointers (animation, triggers)
			// will become stale. If the address is recycled by a new clip, stale
			// entries would cause crashes. Erase everything for this clip.
			{
				std::unique_lock lock(s_triggersBackupMutex);
				s_triggersBackup.erase(a_this);
			}
			{
				std::unique_lock lock(s_originalAnimMutex);
				s_originalAnimMap.erase(a_this);
			}
			{
				std::unique_lock lock(s_clipSuffixMutex);
				s_clipSuffixCache.erase(a_this);
			}
			{
				std::unique_lock lock(s_clipRealPathMutex);
				s_clipRealPathCache.erase(a_this);
			}
			{
				std::unique_lock lock(s_clipRealPathStateMutex);
				s_clipRealPathAuthoritative.erase(a_this);
				s_clipRealPathAttempts.erase(a_this);
				s_pendingActivateLog.erase(a_this);
			}
			{
				// The per-frame poll rebuilds this map, but erase eagerly so a
				// recycled clip address can't inherit stale player membership.
				std::unique_lock lock(s_playerClipMutex);
				s_playerClipGraph.erase(a_this);
			}
			{
				std::unique_lock lock(s_clipVariantMutex);
				s_clipVariantCache.erase(a_this);
			}
			{
				std::unique_lock lock(s_annotStateMutex);
				s_annotStateMap.erase(a_this);
			}
			{
				std::unique_lock lock(s_bypassMutex);
				s_bypassSet.erase(a_this);
			}
			{
				std::unique_lock lock(s_playOnceDecisionMutex);
				s_playOnceDecision.erase(a_this);
			}
			{
				std::unique_lock lock(s_loopEchoFlagMutex);
				s_clipLoopPending.erase(a_this);
				s_clipEchoPending.erase(a_this);
			}
			{
				std::unique_lock lock(s_deactDelayMutex);
				s_deactivationDelay.erase(a_this);
			}
			// Fire custom "on end" events at deactivation + reset variant state if kOnEachPlay
			{
				std::shared_lock smLock(s_activeSubModMutex);
				auto smIt = s_activeSubModMap.find(a_this);
				if (smIt != s_activeSubModMap.end() && smIt->second) {
					auto* deactRefr = GetRefrFromContext(a_context);
					if (!deactRefr) deactRefr = RE::PlayerCharacter::GetSingleton();
					if (deactRefr) {
						QueueCustomEvents(deactRefr, smIt->second->eventsOnEnd, "onEnd/deactivate");

						// kOnEachPlay uses per-clip cache (s_clipVariantCache) which is
						// erased above — no actor-keyed reset needed.
					}
				}
			}
			{
				std::unique_lock smLock(s_activeSubModMutex);
				s_activeSubModMap.erase(a_this);
				s_activeSubModBinding.erase(a_this);
			}
			{
				std::lock_guard rg(s_triggersRestoredMutex);
				s_triggersRestoredSet.erase(a_this);
			}
			// Remove this clip from any track filter source sets to prevent stale
			// pointers from being used if a new clip is allocated at the same address.
			// Do NOT erase the entry when sourceClips becomes empty — keep the cached
			// override alive so non-source clips continue receiving the correct values
			// during animation transitions (e.g., idle→fire→idle). The staleness
			// mechanism (kTrackFilterStaleFrames) will clean up entries that never get
			// a new source clip registered.
			if (s_trackFilterActiveCount.load(std::memory_order_relaxed) > 0) {
				std::unique_lock tfLock(s_trackFilterMutex);
				for (auto& [actor, states] : s_charTrackFilterMap) {
					for (auto& state : states) {
						state.sourceClips.erase(a_this);
						if (state.sourceClip == a_this) state.sourceClip = nullptr;
					}
				}
			}
		}

		Hooks::ClipGeneratorHooks::_Deactivate(a_this, a_context);
	}

	void hkbClipGenerator_Generate(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context,
		const RE::hkbGeneratorOutput** a_activeChildrenOutput, RE::hkbGeneratorOutput& a_output, float a_timeOffset)
	{
		Hooks::ClipGeneratorHooks::_Generate(a_this, a_context, a_activeChildrenOutput, a_output, a_timeOffset);

		// --- Full-body replacement blending ---
		// One-shot _Generate captures the "other" pose on the first blend frame only.
		// All subsequent frames use the frozen snapshot. Only ownerClip applies.
		// All data is copied out under lock before use — no dangling pointers.
		if (s_fullBodyBlendActiveCount.load(std::memory_order_relaxed) > 0) {
			auto* fbActor = GetRefrFromContext(a_context);
			if (!fbActor) fbActor = RE::PlayerCharacter::GetSingleton();
			if (fbActor) {
				std::string clipSuffix;
				{
					std::shared_lock lock(s_clipSuffixMutex);
					auto cit = s_clipSuffixCache.find(a_this);
					if (cit != s_clipSuffixCache.end()) clipSuffix = cit->second;
				}
				if (!clipSuffix.empty()) {
					ActorClipKey fbKey{ fbActor, clipSuffix };

					float fbAlpha = 0.0f;
					bool fbBlendingIn = false, fbBlendingOut = false;
					bool fbNeedSnapshot = false;
					RE::hkaAnimation* fbOrigAnim = nullptr;
					RE::hkaAnimation* fbRepAnim = nullptr;
					thread_local std::vector<RE::hkQsTransformRaw> tl_snapshot;

					{
						std::shared_lock fbLock(s_fullBodyBlendMutex);
						auto fbIt = s_fullBodyBlendMap.find(fbKey);
						if (fbIt != s_fullBodyBlendMap.end() && fbIt->second.ownerClip == a_this) {
							auto& bs = fbIt->second;
							fbAlpha = bs.blendAlpha;
							fbBlendingIn = bs.blendingIn;
							fbBlendingOut = bs.blendingOut;
							fbNeedSnapshot = !bs.poseSnapshotValid;
							fbOrigAnim = bs.original;
							fbRepAnim = bs.replacement;
							if (bs.poseSnapshotValid && !bs.poseSnapshot.empty())
								tl_snapshot = bs.poseSnapshot;
						}
					}

					// Boundary alphas are INCLUDED on purpose — they are not no-ops.
					// Blend-in frame 1 has alpha=0.0 with the replacement already in
					// the slot: the apply (lerp weight 1.0 toward the original-pose
					// snapshot) is what makes that frame render as the original.
					// Blend-out frame 1 has alpha=1.0 with the original already back
					// in the slot: the apply is what keeps that frame rendering the
					// replacement. Excluding them (the old > 0.001 && < 0.999 gate)
					// produced a one-frame flash of the wrong pose at the start of
					// every blend, in both directions (verified from the 2026-07-30
					// session log: first blend-in apply was a=0.003 one frame AFTER
					// the swap, so the swap frame rendered the raw replacement).
					// The far boundary of each direction stays excluded: there the
					// lerp genuinely converges to the live pose, and blend-out
					// entries at alpha<=0.001 are erased by the driver anyway.
					const bool fbApplicable =
						(fbBlendingIn && fbAlpha < 0.999f) ||
						(fbBlendingOut && fbAlpha > 0.001f);
					if (fbApplicable && fbOrigAnim && fbRepAnim) {
						auto* tracksPtr = *reinterpret_cast<uint8_t**>(&a_output);
						if (tracksPtr) {
							auto* headers = reinterpret_cast<RE::TrackHeaderRaw*>(tracksPtr + sizeof(RE::TrackMasterHeaderRaw));
							auto& poseHeader = headers[RE::kTrackIndex_Pose];
							if (poseHeader.numData > 0 && poseHeader.dataOffset > 0) {
								auto* outputPose = reinterpret_cast<RE::hkQsTransformRaw*>(tracksPtr + poseHeader.dataOffset);
								int16_t numBones = poseHeader.numData;

								if (fbNeedSnapshot) {
									// One-shot: save live output, swap to "other" anim, re-generate, swap back.
									thread_local std::vector<RE::hkQsTransformRaw> tl_livePose;
									tl_livePose.assign(outputPose, outputPose + numBones);

									auto** animSlot = a_this->GetAnimationSlot();
									if (animSlot) {
										RE::hkaAnimation* saved = *animSlot;
										RE::hkaAnimation* other = fbBlendingIn ? fbOrigAnim : fbRepAnim;
										if (saved != other) {
											*animSlot = other;
											Hooks::ClipGeneratorHooks::_Generate(a_this, a_context, a_activeChildrenOutput, a_output, a_timeOffset);
											*animSlot = saved;

											// outputPose now has the "other" pose — store as snapshot
											tl_snapshot.assign(outputPose, outputPose + numBones);
											{
												std::unique_lock fbLock2(s_fullBodyBlendMutex);
												auto fbIt2 = s_fullBodyBlendMap.find(fbKey);
												if (fbIt2 != s_fullBodyBlendMap.end() && fbIt2->second.ownerClip == a_this) {
													fbIt2->second.poseSnapshot = tl_snapshot;
													fbIt2->second.poseSnapshotValid = true;
												}
											}

											// Restore the live pose back to output
											std::memcpy(outputPose, tl_livePose.data(), numBones * sizeof(RE::hkQsTransformRaw));

											static int s_snapLog = 0;
											if (s_snapLog < 10) {
												logger::info("[OAR-FullBodyBlend] Snapshot captured: {} bones, in={}", numBones, fbBlendingIn);
												s_snapLog++;
											}
										}
									}
								}

								// Apply blend: lerp live output against frozen snapshot (local copy, safe)
								if (!tl_snapshot.empty() && static_cast<int16_t>(tl_snapshot.size()) == numBones) {
									if (fbBlendingIn) {
										// snapshot = original (frozen), output = replacement (live)
										// alpha 0→1: mix from original toward replacement
										for (int16_t i = 0; i < numBones; ++i)
											LerpTransform(outputPose[i], tl_snapshot[i], 1.0f - fbAlpha);
									} else {
										// snapshot = replacement (frozen), output = original (live)
										// alpha 1→0: mix from replacement toward original
										for (int16_t i = 0; i < numBones; ++i)
											LerpTransform(outputPose[i], tl_snapshot[i], fbAlpha);
									}

									static int s_fbApplyLog = 0;
									if (s_fbApplyLog < 15) {
										logger::info("[OAR-FullBodyBlend] Applied: a={:.3f} in={} out={}", fbAlpha, fbBlendingIn, fbBlendingOut);
										s_fbApplyLog++;
									}
								}
							}
						}
					}
					tl_snapshot.clear();
				}
			}
		}

		// Fast path: skip all locking if no track filter is active anywhere
		if (s_trackFilterActiveCount.load(std::memory_order_relaxed) <= 0) return;

		auto* character = a_context ? a_context->character : nullptr;
		if (!character) return;

		// Use the cache-aware resolver. On miss it re-registers the player's
		// graphs (so newly-spawned graphs become tracked). Returns nullptr for
		// NPCs whose characters aren't in our cache — for those we MUST NOT
		// fall back to "assume player", or we'd apply the player's filter to
		// nearby NPCs' skeletons.
		auto* actor = GetRefrFromContext(a_context);
		if (!actor) return;

		// Get output pose pointers up-front (no lock needed).
		auto* tracksPtr = *reinterpret_cast<uint8_t**>(&a_output);
		if (!tracksPtr) return;
		auto* headers = reinterpret_cast<RE::TrackHeaderRaw*>(tracksPtr + sizeof(RE::TrackMasterHeaderRaw));
		auto& poseHeader = headers[RE::kTrackIndex_Pose];
		if (poseHeader.numData <= 0 || poseHeader.dataOffset <= 0) return;
		// CRITICAL: Never force an inactive clip to become active. If onFraction is 0,
		// Do NOT early-out on onFraction <= 0 here. The non-source paths handle
		// this per-clip: if a clip is truly inactive the engine won't blend it in,
		// but we still need to process source clips so their cache is populated.
		auto* outputPose = reinterpret_cast<RE::hkQsTransformRaw*>(tracksPtr + poseHeader.dataOffset);
		int16_t numOutputBones = poseHeader.numData;

		// One-shot diagnostic: log the pose track header so we can verify layout
		// (size of element, capacity, onFraction, flags). The flags byte tells us
		// if the pose is sparse/palette, and onFraction must be > 0 for the engine
		// to consider this track valid downstream.
		static int s_poseHdrLog = 0;
		if (s_poseHdrLog < 3) {
			logger::info("[OAR-TrackFilter] poseHeader: numData={} capacity={} elemSize={} dataOff={} onFrac={:.3f} flags=0x{:02X} type={}",
				poseHeader.numData, poseHeader.capacity, poseHeader.elementSizeBytes,
				poseHeader.dataOffset, poseHeader.onFraction,
				static_cast<uint8_t>(poseHeader.flags), poseHeader.type);
			s_poseHdrLog++;
		}

		// Snapshot which filters are registered for this actor, and bail out
		// entirely if this clip's slot currently holds ANY registered replacement
		// — that means we're inside a swap-fallback's recursive _Generate and
		// must not re-enter our own logic.
		std::vector<const SubMod::TrackFilter*> actorFilters;
		{
			std::shared_lock tfShared(s_trackFilterMutex);
			auto mapIt = s_charTrackFilterMap.find(actor);
			if (mapIt == s_charTrackFilterMap.end()) return;
			auto** slotOuter = a_this->GetAnimationSlot();
			actorFilters.reserve(mapIt->second.size());
			for (auto& st : mapIt->second) {
				if (slotOuter && *slotOuter && st.replacement && *slotOuter == st.replacement) return;
				if (st.filter) actorFilters.push_back(st.filter);
			}
		}

		// Apply every active filter independently — each has its own bone set,
		// weight, blend alpha and cached poses. Overlapping bones resolve in
		// registration order (later filters win). Inside the lambda, `return`
		// means "done with THIS filter", not "done with the clip".
		auto processFilter = [&](const SubMod::TrackFilter* filterKey) {
		// --- Try shared_lock first for the fast non-source path ---
		// Non-source clips only READ cached data. We avoid unique_lock contention
		// that causes freezes when many clips fire during weapon events.
		{
			std::shared_lock tfShared(s_trackFilterMutex);
			auto* statePtr = FindTrackFilterState(actor, filterKey);
			if (!statePtr) return;

			auto& state = *statePtr;
			auto* filterPtr = state.filter;
			auto* replacement = state.replacement;
			if (!filterPtr || !filterPtr->enabled || !replacement) return;

			bool isSourceClip = (state.sourceClips.count(a_this) > 0);
			const auto currentFrame = s_currentFrame.load(std::memory_order_relaxed);
			const bool standaloneNeedsSample = state.standaloneSpecialIdle &&
				state.lastStandaloneSampleFrame != currentFrame &&
				(poseHeader.flags & 0x01) == 0 && poseHeader.onFraction > 0.0f;

			// Non-source clips with valid cache and already-resolved bones:
			// handle entirely under shared_lock (no writes to shared state).
			// Condition re-evaluation in HookedActorUpdate handles cleanup when
			// conditions become false, so no staleness check is needed here.
			auto charIt = state.resolvedByChar.find(character);
			bool alreadyResolved = (charIt != state.resolvedByChar.end() &&
				charIt->second.version == filterPtr->version.load(std::memory_order_relaxed) &&
				!charIt->second.nameAndIndex.empty());

			if (!isSourceClip && !standaloneNeedsSample && state.cacheValid && alreadyResolved) {
				if (ShouldSkipAddNonSourceClip(a_this, filterPtr)) return;

				// Skip clips with onFraction=0 — their pose buffer is uninitialized.
				if (poseHeader.onFraction <= 0.f) return;

				// NON-SOURCE POLICY:
				// The track filter's purpose is applying a partial-bone override
				// (e.g., pistol slide locked back) across all normal animations
				// (walk, fire, turn, idle) without needing separate assets for each.
				//
				// - Non-additive clips: override target bones with cached replacement.
				// - Additive clips: zero target bones (suppress sway/jiggle deltas).
				// - EXCEPTION — sneak-related clips: skip entirely. The sneak offset
				//   additive provides the crouch positional delta; zeroing it prevents
				//   the weapon from following the camera downward → white screen.
				//   Sneak non-additive clips similarly shouldn't be overridden with
				//   standing-pose values during crouch transitions.
				std::string clipSuffix;
				{
					std::shared_lock csLock(s_clipSuffixMutex);
					auto csIt = s_clipSuffixCache.find(a_this);
					if (csIt != s_clipSuffixCache.end())
						clipSuffix = csIt->second;
				}

				// Skip sneak-related clips — let them keep natural bone positions.
				if (!clipSuffix.empty()) {
					auto leafView = GetSuffixLeaf(clipSuffix);
					// Only skip clips whose leaf STARTS with "sneak" (e.g., sneakoffset,
					// sneakforward). Jiggle clips like "wpnafterjigglesneakdown" don't
					// start with "sneak" and should still be processed (zeroed).
					if (leafView.size() >= 5 && leafView.substr(0, 5) == "sneak") return;
				}

				const bool isAdditiveClip = (poseHeader.flags & 0x01) != 0;
				auto& cr = charIt->second;
				float weight = filterPtr->weight * state.blendAlpha;
				if (weight <= 0.001f) return;
				auto mode = filterPtr->mode;

				// Diagnostic: confirm non-source clips are applying blend-out weight
				if (state.blendingOut) {
					static int s_nsBoLog = 0;
					if (s_nsBoLog < 20) {
						logger::info("[OAR-TrackFilter] NonSrc-FastPath BLEND-OUT: suffix='{}' clip={:X} additive={} weight={:.4f} alpha={:.4f} onFrac={:.3f}",
							state.suffix, reinterpret_cast<uintptr_t>(a_this),
							isAdditiveClip, weight, state.blendAlpha, poseHeader.onFraction);
						s_nsBoLog++;
					}
				}

				for (auto& [name, idx] : cr.nameAndIndex) {
					// Camera motion belongs only to the source clip. Pasting one
					// absolute cached Camera local into every concurrently generating
					// clip can duplicate it and can leave a foreign weapon's camera
					// placement alive after the source has ended.
					if (IsTrackFilterCameraBone(name)) continue;
					if (idx < 0 || idx >= numOutputBones) continue;

					if (isAdditiveClip) {
						RE::hkQsTransformRaw identity{};
						identity.translation[0] = 0.f; identity.translation[1] = 0.f;
						identity.translation[2] = 0.f; identity.translation[3] = 0.f;
						identity.rotation[0] = 0.f; identity.rotation[1] = 0.f;
						identity.rotation[2] = 0.f; identity.rotation[3] = 1.f;
						identity.scale[0] = 1.f; identity.scale[1] = 1.f;
						identity.scale[2] = 1.f; identity.scale[3] = 0.f;
						outputPose[idx] = identity;
					} else {
						auto rIt = state.cachedRepByName.find(name);
						if (rIt == state.cachedRepByName.end()) continue;
						if (mode == SubMod::TrackFilter::Mode::Override) {
							LerpTransform(outputPose[idx], rIt->second, weight);
						} else {
							auto bIt = state.cachedBaseByName.find(name);
							if (bIt != state.cachedBaseByName.end()) {
								BlendAdditiveTransform(outputPose[idx], bIt->second, rIt->second, weight);
							} else {
								LerpTransform(outputPose[idx], rIt->second, weight);
							}
						}
					}
					SetPoseBoneMaskBit(tracksPtr, poseHeader, idx);
				}

				// Frozen bones: hold the captured local (additive clips get the
				// identity so sway cannot wiggle a frozen bone). Skipped until
				// the source clip's first sample captures the value.
				for (auto& [name, idx] : cr.freezeNameAndIndex) {
					if (IsTrackFilterCameraBone(name)) continue;
					if (idx < 0 || idx >= numOutputBones) continue;
					if (isAdditiveClip) {
						RE::hkQsTransformRaw identity{};
						identity.translation[0] = 0.f; identity.translation[1] = 0.f;
						identity.translation[2] = 0.f; identity.translation[3] = 0.f;
						identity.rotation[0] = 0.f; identity.rotation[1] = 0.f;
						identity.rotation[2] = 0.f; identity.rotation[3] = 1.f;
						identity.scale[0] = 1.f; identity.scale[1] = 1.f;
						identity.scale[2] = 1.f; identity.scale[3] = 0.f;
						outputPose[idx] = identity;
					} else {
						auto fIt = state.frozenByName.find(name);
						if (fIt == state.frozenByName.end()) continue;
						LerpTransform(outputPose[idx], fIt->second, weight);
					}
					SetPoseBoneMaskBit(tracksPtr, poseHeader, idx);
				}

				static int s_nonSrcFastLog = 0;
				if (s_nonSrcFastLog < 5) {
					int dumped = 0;
					for (auto& [name, idx] : cr.nameAndIndex) {
						if (dumped++ >= 2) break;
						if (idx < 0 || idx >= numOutputBones) continue;
						auto& f = outputPose[idx];
						logger::info("[OAR-TrackFilter] FINAL nonsrc(fast) '{}'[{}]: trans=({:.3f},{:.3f},{:.3f}) additive={}",
							name, idx, f.translation[0], f.translation[1], f.translation[2], isAdditiveClip);
					}
					s_nonSrcFastLog++;
				}
				return;
			}
		} // shared_lock released

		// Acquire UNIQUE lock for source clips, first-time resolution, or stale checks.
		std::unique_lock tfLock(s_trackFilterMutex);

		auto* statePtr = FindTrackFilterState(actor, filterKey);
		if (!statePtr) return;

		auto& state = *statePtr;
		auto* filterPtr = state.filter;
		auto* replacement = state.replacement;
		if (!filterPtr || !filterPtr->enabled || !replacement) return;


		// --- Per-character resolution (Issue #1 fix) ---
		auto& cr = state.resolvedByChar[character];
		uint64_t filterVersion = filterPtr->version.load(std::memory_order_relaxed);
		if (cr.version != filterVersion) {
			ResolveForChar(filterPtr, cr, character);
		}
		if (cr.nameAndIndex.empty()) return;

		float weight = filterPtr->weight * state.blendAlpha;
		auto mode = filterPtr->mode;
		const float nowSec = s_tfNowSec.load(std::memory_order_relaxed);
		const auto currentFrame = s_currentFrame.load(std::memory_order_relaxed);
		const bool standaloneSampler = state.standaloneSpecialIdle &&
			state.lastStandaloneSampleFrame != currentFrame &&
			(poseHeader.flags & 0x01) == 0 && poseHeader.onFraction > 0.0f;
		bool isSourceClip = (state.sourceClips.count(a_this) > 0) || standaloneSampler;

		// End Clip If Shorter + end-driven fade: the fading alpha must only
		// apply to NON-source clips. On the incoming state's clips, fading the
		// stamp off IS the blend into the state transition (donor pose -> next
		// animation). But on the SOURCE clip the same fade reveals the ORIGINAL
		// underneath — the one animation that must never surface here, and one
		// the engine's exit crossfade is already fading out wholesale. Pin the
		// source-side stamp at full weight; the crossfade disposes of it.
		// Condition-driven fades (conditions failed mid-play, clip continues)
		// keep fading the source too — there the original is the right target.
		if (isSourceClip && state.parentSubMod && state.parentSubMod->GetEndClipIfShorter() &&
			(state.earlyBlendOutArmed || state.oneShotDone)) {
			weight = filterPtr->weight;
		}
		// Source clips must proceed even at zero weight: dormant one-shot states
		// watch the source's localTime here to detect a restart, and a blend-in's
		// first frame still primes the sample cache. Non-source clips only apply
		// cached values, so zero weight means nothing to do.
		if (weight <= 0.001f && !isSourceClip) return;
		auto** animSlot = a_this->GetAnimationSlot();

		static int s_genEntryLog = 0;
		if (s_genEntryLog < 6) {
			logger::info("[OAR-TrackFilter] Generate: char={:X} a_this={:X} isSource={} bones={}",
				reinterpret_cast<uintptr_t>(character),
				reinterpret_cast<uintptr_t>(a_this),
				isSourceClip, cr.nameAndIndex.size());
			s_genEntryLog++;
		}

		// =====================================================================
		// SOURCE CLIP PATH
		//
		// Preferred path: read the binding's transformTrackToBoneIndices, sample
		// the replacement directly via SamplePartialTracks, look up the right
		// track per filtered bone, and apply. This is the "correct track / right
		// joint" path.
		//
		// Fallback path: if no binding can be read for this clip, fall back to
		// the swap-and-recursive-Generate approach so we don't regress to "no
		// override at all". This matches the previous behavior.
		// =====================================================================
		if (isSourceClip) {
			// If the engine has set onFraction=0, this clip is being deactivated
			// (state transition). Respect that — do NOT sample or force it active.
			// Otherwise we keep a stale standing pose at full weight during transitions.
			if (poseHeader.onFraction <= 0.f) return;

			RE::hkaAnimation* repAnim = replacement;
			if (!repAnim) return;

			const auto* trackToBoneArr = GetTrackToBoneIndices(a_this);
			const bool haveMapping =
				trackToBoneArr && trackToBoneArr->data && trackToBoneArr->size > 0;

			// An EMPTY transformTrackToBoneIndices array is not "no binding":
			// Havok's convention is that an empty array means IDENTITY mapping
			// (track i drives bone i). The clip's animation slot lives on this
			// same binding object (binding+0x18), so if the slot is readable the
			// binding exists and direct sampling with identity mapping is valid.
			// Previously an empty array sent these clips to the swap-and-generate
			// fallback, which follows playback time and therefore cannot honor
			// fixed-frame sampling (seen on 'Sig Idle Empty' / p226 wpnidleready).
			const bool haveIdentityBinding = !haveMapping && animSlot && *animSlot;

			// The donor's OWN track->bone map wins over the host clip's binding:
			// the host binding describes the HOST animation's track layout,
			// which only matches the donor's when the donor was authored for
			// this weapon's animation set. A Leaf Matching donor sampled under
			// another weapon's clip through the host mapping put wrong tracks
			// on wrong bones (the MCX glitch, 2026-08-16).
			const int16_t* donorMap =
				state.donorTrackToBone.empty() ? nullptr : state.donorTrackToBone.data();
			const int32_t donorMapSize = static_cast<int32_t>(state.donorTrackToBone.size());
			const bool donorIdentity = state.donorMapIdentity;

			static int s_pathLog = 0;
			if (s_pathLog < 6) {
				logger::info("[OAR-TrackFilter] Source path: clip={:X} mapping={} identity={} bindingTracks={} donorMap={} donorIdentity={}",
					reinterpret_cast<uintptr_t>(a_this), haveMapping, haveIdentityBinding,
					haveMapping ? trackToBoneArr->size : 0, donorMapSize, donorIdentity);
				s_pathLog++;
			}

			if (donorMap || donorIdentity || haveMapping || haveIdentityBinding) {
				// ============== Direct sampling path ==============
				const auto* trackToBoneData = haveMapping
					? reinterpret_cast<const int16_t*>(trackToBoneArr->data) : nullptr;
				const int32_t bindingNumTracks = haveMapping ? trackToBoneArr->size : 0;
				const int32_t animNumTracks = repAnim->numberOfTransformTracks;
				const int32_t numTracksToSample = haveMapping
					? std::min(animNumTracks, bindingNumTracks) : animNumTracks;
				if (numTracksToSample <= 0) return;

				float localTime = state.standaloneSpecialIdle ?
					std::max(0.0f, nowSec - state.standaloneStartSec) : a_this->GetLocalTime();
				float repDuration = repAnim->duration;
				const bool loopingSource = !state.standaloneSpecialIdle &&
					(a_this->mode == RE::MODE_LOOPING);

				if (filterPtr->sampleFrame >= 0.0f) {
					// Fixed-frame sampling: hold the override pose at one authored
					// frame of the replacement instead of following playback time.
					// FO4 animations are authored at 30 fps. Clamp (don't wrap) so a
					// frame past the end holds the final pose.
					constexpr float kAnimFps = 30.0f;
					localTime = filterPtr->sampleFrame / kAnimFps;
					if (repDuration > 0.001f && localTime > repDuration) {
						localTime = repDuration;
					}
					if (state.standaloneSpecialIdle && repDuration > 0.001f) {
						const float elapsed = std::max(0.0f, nowSec - state.standaloneStartSec);
						if (!filterPtr->blendOutAtEnd && filterPtr->blendOutTime > 0.0f &&
							!state.earlyBlendOutArmed &&
							elapsed >= repDuration - filterPtr->blendOutTime) {
							state.earlyBlendOutArmed = true;
						}
						if (elapsed >= repDuration) state.oneShotDone = true;
					}
				} else if (repDuration > 0.001f) {
					if (loopingSource) {
						// Looping source (sprint, walk overlays): wrap the clip's
						// localTime to the replacement's duration so different-length
						// replacements cycle correctly.
						localTime = std::fmod(localTime, repDuration);
						if (localTime < 0.f) localTime += repDuration;
					} else if (state.standaloneSpecialIdle) {
						// No graph clip owns this playback. The intercepted PlayIdle
						// request supplied the start time and the donor runs once.
						state.lastAdvanceSec = nowSec;
						state.lastSampledLocalTime = localTime;
						if (!filterPtr->blendOutAtEnd && filterPtr->blendOutTime > 0.0f &&
							!state.earlyBlendOutArmed &&
							localTime >= repDuration - filterPtr->blendOutTime) {
							state.earlyBlendOutArmed = true;
						}
						if (localTime >= repDuration) {
							localTime = repDuration;
							state.oneShotDone = true;
						}
					} else {
						// ---- One-shot source (SINGLE_PLAY clip) ----
						// Restart detection first: localTime moving backward means the
						// engine restarted this generator for a new play. Re-arm a
						// finished/dormant overlay and blend back in.
						if (state.lastSampledLocalTime >= 0.0f &&
							localTime + 0.05f < state.lastSampledLocalTime) {
							// An armed end-anchored fade counts as ended even before it
							// reaches dormancy: re-throwing during the fade is a new
							// play and must re-fire eventsOnStart.
							const bool wasEnded = state.dormant || state.oneShotDone || state.earlyBlendOutArmed;
							state.oneShotDone = false;
							state.sampleStarved = false;
							state.frozenByName.clear();
							state.lastAdvanceSec = nowSec;
							state.selfAdvanceStartSec = -1.0f;
							state.selfAdvanceBaseTime = 0.0f;
							state.earlyBlendOutArmed = false;
							if (state.dormant || state.blendingOut) {
								state.dormant = false;
								state.blendingOut = false;
								float blendIn = filterPtr->blendInTime;
								state.blendDuration = blendIn;
								state.blendElapsed = (blendIn > 0.0f)
	? InverseBlendCurve(CurveOf(state.parentSubMod), state.blendAlpha) * blendIn : 0.0f;
								if (blendIn <= 0.0f) state.blendAlpha = 1.0f;
							}
							if (wasEnded) {
								state.onEndFired = false;
								if (state.parentSubMod) {
									QueueCustomEvents(actor, state.parentSubMod->eventsOnStart, "onStart/trackFilter-restart");
								}
							}
						}
						if (std::fabs(localTime - state.lastSampledLocalTime) > 1e-5f) {
							state.lastAdvanceSec = nowSec;
						}
						state.lastSampledLocalTime = localTime;

						// One-shot end handling. The one-shot ends ONLY when the
						// DONOR has played through (clamp, never wrap: a donor
						// shorter than the original must hold its final frame while
						// blending out, not snap back to frame 0 mid-play).
						//
						// A SOURCE that finishes or parks first does NOT end the
						// overlay anymore — the graph holds the finished clip active
						// and keeps sampling (2026-08-04 session), so the donor
						// switches to wall-clock self-advance and plays out its
						// remaining content. Ending the one-shot here instead cut a
						// long donor off at the short vanilla clip's end (KV
						// Broadside grenade throw blended out at 1.29s); before
						// that, doing nothing froze the overlay on the parked frame.
						const float srcDuration = (animSlot && *animSlot) ? (*animSlot)->duration : 0.0f;
						const bool sourceDone = srcDuration > 0.02f && localTime >= srcDuration - 0.02f;
						const bool stalled = state.lastAdvanceSec > 0.0f &&
							nowSec - state.lastAdvanceSec > kOneShotStallSeconds;

						// OVERRIDE CONTRACT: the donor must look IDENTICAL no
						// matter which weapon's clip hosts the play. Host clips
						// differ per weapon in playback speed, start offset, and
						// park point, and every one of those used to shape the
						// donor through the host's localTime. In Override mode
						// the donor therefore runs on OAR's own clock from the
						// play's first sample — the same self-advance clock that
						// already takes over when a source parks — and the host
						// clip only starts, restarts (localTime regression
						// above), and ends the state (starvation/deactivation).
						// The restart paths reset selfAdvanceStartSec to -1,
						// which lands here as "restart the override clock".
						// Additive mode keeps host time (its deltas belong to
						// the host animation); looping sources keep loop sync
						// (cycle-authored overlays must track the host cycle);
						// fixed-frame sampling never reaches this branch.
						if (mode == SubMod::TrackFilter::Mode::Override &&
							state.selfAdvanceStartSec < 0.0f) {
							state.selfAdvanceStartSec = nowSec;
							state.selfAdvanceBaseTime = 0.0f;
							static std::atomic<int> s_ovClockLog{ 0 };
							if (s_ovClockLog.fetch_add(1, std::memory_order_relaxed) < 20) {
								logger::info("[OAR-TrackFilter] Override clock started for '{}' (host t={:.3f} ignored from here on)",
									state.suffix, localTime);
							}
						}

						if (state.selfAdvanceStartSec >= 0.0f) {
							// Donor on its own clock (Override contract, or a
							// parked source in Additive mode).
							localTime = state.selfAdvanceBaseTime + (nowSec - state.selfAdvanceStartSec);
						}

						// End-anchored blend-out (default): start the fade
						// blendOutTime BEFORE the donor's final frame so it
						// COMPLETES on that frame, with the donor still animating
						// underneath. blendOutAtEnd restores the legacy behavior of
						// starting the fade at the end and running past it.
						//
						// This applies with End Clip If Shorter too (author's call,
						// 2026-08-14): the fade runs donor -> source over the last
						// blendOutTime and lands at zero right as the re-timed
						// transitions exit the state, so the handoff into the next
						// state starts from the source pose. Ticking Blend Out After
						// End instead keeps the donor at full weight to its end and
						// fades during the engine's crossfade to the next state.
						if (!filterPtr->blendOutAtEnd && filterPtr->blendOutTime > 0.0f &&
							!state.earlyBlendOutArmed &&
							localTime >= repDuration - filterPtr->blendOutTime) {
							state.earlyBlendOutArmed = true;
						}

						const bool donorDone = localTime >= repDuration;
						if (donorDone) {
							localTime = repDuration;
							state.oneShotDone = true;
						} else if ((sourceDone || stalled) && state.selfAdvanceStartSec < 0.0f) {
							state.selfAdvanceStartSec = nowSec;
							state.selfAdvanceBaseTime = localTime;
							logger::info("[OAR-TrackFilter] Source {} at t={:.3f} before donor end ({:.3f}s) for '{}' — donor self-advancing to completion",
								sourceDone ? "finished" : "stalled", localTime, repDuration, state.suffix);
						}
					}
				}

				// Dormant handling: starvation-induced dormancy (source stopped
				// sampling) wakes as soon as samples resume; a completed one-shot
				// (oneShotDone) stays dormant until the restart detection above
				// sees the clip's localTime jump backward.
				if (state.dormant) {
					if (state.sampleStarved && !state.oneShotDone && !state.earlyBlendOutArmed) {
						state.dormant = false;
						state.sampleStarved = false;
						state.blendingOut = false;
						float blendIn = filterPtr->blendInTime;
						state.blendDuration = blendIn;
						state.blendElapsed = 0.0f;
						state.blendAlpha = (blendIn <= 0.0f) ? 1.0f : 0.0f;
						if (state.onEndFired && state.parentSubMod) {
							state.onEndFired = false;
							QueueCustomEvents(actor, state.parentSubMod->eventsOnStart, "onStart/trackFilter-restart");
						}
					} else {
						// A native source clip can remain alive while dormant. A
						// standalone special idle has no source to keep alive, so do
						// not refresh it and let normal staleness cleanup retire it.
						if (!state.standaloneSpecialIdle) {
							state.lastSourceTimeSec = nowSec;
						}
						return;
					}
				} else if (state.sampleStarved) {
					// Samples resumed during a starvation blend-out: cancel it and
					// blend back in from the current alpha (no pop).
					state.sampleStarved = false;
					if (state.blendingOut) {
						state.blendingOut = false;
						float blendIn = filterPtr->blendInTime;
						state.blendDuration = blendIn;
						state.blendElapsed = (blendIn > 0.0f)
	? InverseBlendCurve(CurveOf(state.parentSubMod), state.blendAlpha) * blendIn : 0.0f;
						if (blendIn <= 0.0f) state.blendAlpha = 1.0f;
					}
				}

				// Sanity: numberOfTransformTracks read at +0x18 — the same field
				// the cache logs as "tracks=N" at load, so a wild value here
				// means a corrupted/stale animation pointer. Bail rather than
				// hand the engine an undersized output buffer.
				if (animNumTracks > 4096) {
					static int s_trackCountWarn = 0;
					if (s_trackCountWarn < 5) {
						logger::warn("[OAR-TrackFilter] Implausible track count {} on replacement — skipping sample", animNumTracks);
						s_trackCountWarn++;
					}
					return;
				}

				// sampleTracks fills ALL of the animation's tracks (it reads the
				// counts from the animation itself — see SampleTracks in
				// HavokTypes.h), so the buffers are sized by the animation's own
				// track counts, independent of how many tracks we then map.
				thread_local std::vector<RE::hkQsTransformRaw> tl_sampledTracks;
				thread_local std::vector<float> tl_sampledFloats;
				tl_sampledTracks.assign(animNumTracks, RE::hkQsTransformRaw{});
				tl_sampledFloats.assign(std::max(1, repAnim->numberOfFloatTracks), 0.0f);

				repAnim->SampleTracks(localTime, tl_sampledTracks.data(), tl_sampledFloats.data());

				static int s_bindingDiagLog = 0;
				bool wantBindingDiag = (s_bindingDiagLog < 3);
				if (wantBindingDiag) {
					logger::info("[OAR-TrackFilter-Binding] char={:X} bindingTracks={} animTracks={} localTime={:.3f}",
						reinterpret_cast<uintptr_t>(character),
						bindingNumTracks, animNumTracks, localTime);
				}

				// ---- Model-space anchoring setup (Override + playback-following) ----
				// Donor LOCALS under the base animation's (different) parent chain do
				// not reproduce the donor's motion: the arm inherits the base anim's
				// torso. For each chain ROOT (filtered bone whose skeleton parent is
				// outside the filter set), replace the donor local with
				//   inv(currentParentModel) * donorModel(bone)
				// so the whole chain lands exactly where the donor puts it relative
				// to the character root. Children keep raw donor locals — composed
				// under the anchored root they reproduce the donor in model space.
				bool doAnchor = filterPtr->modelSpaceAnchor &&
					mode == SubMod::TrackFilter::Mode::Override &&
					filterPtr->sampleFrame < 0.0f;
				const bool needsCameraModelSpace = std::ranges::any_of(
					cr.nameAndIndex, [](const auto& a_entry) {
						return IsTrackFilterCameraBone(a_entry.first);
					});
				const int16_t* skelParents = nullptr;
				int32_t skelBoneCount = 0;
				if (doAnchor || needsCameraModelSpace) {
					if (auto* setup = character->setup._ptr) {
						if (auto* skel = reinterpret_cast<uint8_t*>(setup->animationSkeleton._ptr)) {
							auto* parentArr = reinterpret_cast<RE::hkArrayRawLayout*>(skel + RE::kSkeletonOffset_parentIndices);
							if (parentArr->data && parentArr->size > 0) {
								skelParents = reinterpret_cast<int16_t*>(parentArr->data);
								skelBoneCount = parentArr->size;
							}
						}
					}
					if (!skelParents) doAnchor = false;
				}

				thread_local std::unordered_set<int16_t> tl_filteredSet;
				tl_filteredSet.clear();
				if (doAnchor) {
					for (auto& [n2, i2] : cr.nameAndIndex) tl_filteredSet.insert(i2);
				}

				auto donorTrackFor = [&](int16_t boneIdx) -> int32_t {
					if (donorMap) {
						const int32_t n = std::min(donorMapSize, animNumTracks);
						for (int32_t t = 0; t < n; ++t)
							if (donorMap[t] == boneIdx) return t;
						return -1;
					}
					if (donorIdentity) {
						return (boneIdx < animNumTracks) ? static_cast<int32_t>(boneIdx) : -1;
					}
					if (haveMapping) {
						for (int32_t t = 0; t < numTracksToSample; ++t)
							if (trackToBoneData[t] == boneIdx) return t;
						return -1;
					}
					return (boneIdx < numTracksToSample) ? static_cast<int32_t>(boneIdx) : -1;
				};

				// Model transform = compose locals root to bone. Donor versions use the
				// supplied sample and fall back to the same live native locals for
				// untracked ancestors. Using that identical fallback for frame zero and
				// the current donor frame keeps native ancestor motion out of the donor
				// delta while retaining a complete chain.
				auto modelTransform = [&](int16_t boneIdx,
					const std::vector<RE::hkQsTransformRaw>* a_donorTracks,
					RE::hkQsTransformRaw& outXf) -> bool {
					if (!skelParents || boneIdx < 0 || boneIdx >= skelBoneCount) return false;
					int16_t chain[64];
					int n = 0;
					for (int16_t b = boneIdx; b >= 0 && b < skelBoneCount && n < 64; b = skelParents[b])
						chain[n++] = b;
					if (n == 0 || (n == 64 && skelParents[chain[n - 1]] >= 0)) return false;
					RE::hkQsTransformRaw acc = MakeIdentityQs();
					for (int i = n - 1; i >= 0; --i) {
						const int16_t b = chain[i];
						const RE::hkQsTransformRaw* l = nullptr;
						if (a_donorTracks) {
							const int32_t trk = donorTrackFor(b);
							if (trk >= 0 && trk < static_cast<int32_t>(a_donorTracks->size())) {
								l = &(*a_donorTracks)[trk];
							}
						}
						if (!l && b < numOutputBones) l = &outputPose[b];
						if (!l || !IsFiniteQs(*l)) return false;
						ComposeQs(acc, *l, acc);
					}
					outXf = acc;
					return IsFiniteQs(outXf);
				};

				for (auto& [name, idx] : cr.nameAndIndex) {
					if (idx < 0 || idx >= numOutputBones) continue;
					const bool isCameraBone = IsTrackFilterCameraBone(name);

					// Same resolution order as donorTrackFor: donor's own map,
					// donor identity, host mapping, host identity.
					const int32_t trackIdx = donorTrackFor(idx);

					if (wantBindingDiag) {
						logger::info("[OAR-TrackFilter-Binding]   '{}': boneIdx={} trackIdx={} (identity={})",
							name, idx, trackIdx, (trackIdx == idx ? "yes" : "no"));
					}

					if (trackIdx < 0) continue;

					RE::hkQsTransformRaw repVal = tl_sampledTracks[trackIdx];
					RE::hkQsTransformRaw baseVal = outputPose[idx];

					// Camera locals are not portable across weapon graphs because the
					// live and donor parent bases can differ. Reconstruct the donor motion
					// in character model space, convert it into a weighted delta relative
					// to this frame's native Camera local, and publish that delta for the
					// post-animation scene-node hook. Never write Camera into outputPose:
					// doing so lets a malformed Camera track enter Havok's blend tree,
					// which is the path associated with the white-screen session failure.
					if (isCameraBone) {
						if (state.cameraDonorFrameZeroTracks.empty() &&
							!state.invalidCameraReferenceTracks.count(trackIdx)) {
							thread_local std::vector<float> tl_cameraReferenceFloats;
							state.cameraDonorFrameZeroTracks.assign(animNumTracks, RE::hkQsTransformRaw{});
							tl_cameraReferenceFloats.assign(
								std::max(1, repAnim->numberOfFloatTracks), 0.0f);
							repAnim->SampleTracks(
								0.0f, state.cameraDonorFrameZeroTracks.data(), tl_cameraReferenceFloats.data());
							const auto& reference = state.cameraDonorFrameZeroTracks[trackIdx];

							if (IsFiniteQs(reference)) {
								logger::info(
									"[OAR-TrackFilter-Camera] Model-space camera active for '{}' "
									"(bone={}, track={}, donor frame-zero tracks captured)",
									state.suffix, idx, trackIdx);
							} else {
								state.cameraDonorFrameZeroTracks.clear();
								state.invalidCameraReferenceTracks.insert(trackIdx);
								logger::warn(
									"[OAR-TrackFilter-Camera] Invalid donor frame-zero Camera for '{}' - "
									"leaving the native Camera untouched",
									state.suffix);
							}
						}

						// Remove values cached by an earlier activation/build. Camera is
						// intentionally never eligible for non-source propagation.
						state.cachedRepByName.erase(name);
						state.cachedBaseByName.erase(name);

						const uint64_t cameraEvaluation =
							s_activeCameraEvaluation.load(std::memory_order_acquire);
						const bool firstPersonPlayerSample =
							cameraEvaluation != 0 &&
							actor == RE::PlayerCharacter::GetSingleton() &&
							GetPlayingClipPerspective(a_this) == OARClipPerspective::kFirstPerson;

						if (firstPersonPlayerSample && !state.cameraDonorFrameZeroTracks.empty() &&
							IsFiniteQs(repVal) && IsFiniteQs(baseVal) && skelParents && idx < skelBoneCount) {
							RE::hkQsTransformRaw donorReferenceModel;
							RE::hkQsTransformRaw donorCurrentModel;
							RE::hkQsTransformRaw nativeCameraModel;
							const int16_t parentIdx = skelParents[idx];
							RE::hkQsTransformRaw nativeParentModel = MakeIdentityQs();
							const bool haveModels =
								modelTransform(idx, &state.cameraDonorFrameZeroTracks, donorReferenceModel) &&
								modelTransform(idx, &tl_sampledTracks, donorCurrentModel) &&
								modelTransform(idx, nullptr, nativeCameraModel) &&
								(parentIdx < 0 || modelTransform(parentIdx, nullptr, nativeParentModel)) &&
								HasUsableRotation(donorReferenceModel) &&
								HasUsableRotation(donorCurrentModel) &&
								HasUsableRotation(nativeCameraModel) &&
								HasUsableRotation(nativeParentModel);

							if (haveModels && weight > 0.001f) {
								// InverseQuat and InverseQs use conjugates, so normalize the
								// composed rotations before calculating either inverse. A
								// degenerate sampled quaternion becomes identity instead of
								// propagating non-finite values into the render pose.
								NormalizeQuat(donorReferenceModel.rotation);
								NormalizeQuat(donorCurrentModel.rotation);
								NormalizeQuat(nativeCameraModel.rotation);
								NormalizeQuat(nativeParentModel.rotation);
								RE::hkQsTransformRaw targetModel = nativeCameraModel;
								for (int axis = 0; axis < 3; ++axis) {
									targetModel.translation[axis] +=
										(donorCurrentModel.translation[axis] - donorReferenceModel.translation[axis]) * weight;
								}

								float inverseReference[4];
								InverseQuat(donorReferenceModel.rotation, inverseReference);
								float donorModelDelta[4];
								MultiplyQuat(donorCurrentModel.rotation, inverseReference, donorModelDelta);
								NormalizeQuat(donorModelDelta);
								static constexpr float kIdentityQuat[4] = { 0.f, 0.f, 0.f, 1.f };
								float weightedModelDelta[4];
								SlerpQuat(kIdentityQuat, donorModelDelta, weight, weightedModelDelta);
								MultiplyQuat(weightedModelDelta, nativeCameraModel.rotation, targetModel.rotation);
								NormalizeQuat(targetModel.rotation);

								RE::hkQsTransformRaw inverseNativeParent;
								RE::hkQsTransformRaw targetLocal;
								InverseQs(nativeParentModel, inverseNativeParent);
								ComposeQs(inverseNativeParent, targetModel, targetLocal);
								// Camera animation should not introduce scale. Retain the
								// live graph's local scale exactly.
								for (int axis = 0; axis < 4; ++axis) {
									targetLocal.scale[axis] = baseVal.scale[axis];
								}
								if (IsFiniteQs(targetLocal) && HasUsableRotation(baseVal) &&
									HasUsableRotation(targetLocal)) {
									float inverseBaseRotation[4];
									InverseQuat(baseVal.rotation, inverseBaseRotation);
									MultiplyQuat(targetLocal.rotation, inverseBaseRotation,
										state.pendingCameraRotationDelta);
									NormalizeQuat(state.pendingCameraRotationDelta);
									for (int axis = 0; axis < 3; ++axis) {
										state.pendingCameraTranslationDelta[axis] =
											targetLocal.translation[axis] - baseVal.translation[axis];
									}
									state.pendingCameraEvaluation = cameraEvaluation;
									state.pendingCameraValid = true;
									static std::atomic<uint32_t> s_cameraModelDiagCount{ 0 };
									const uint32_t diagIndex = s_cameraModelDiagCount.fetch_add(1);
									if (diagIndex < 12) {
										logger::info(
											"[OAR-TrackFilter-Camera] Published post-eval delta '{}' "
											"modelDT=({:.3f},{:.3f},{:.3f}) localDT=({:.3f},{:.3f},{:.3f}) "
											"evaluation={} weight={:.3f}",
											state.suffix,
											donorCurrentModel.translation[0] - donorReferenceModel.translation[0],
											donorCurrentModel.translation[1] - donorReferenceModel.translation[1],
											donorCurrentModel.translation[2] - donorReferenceModel.translation[2],
											state.pendingCameraTranslationDelta[0],
											state.pendingCameraTranslationDelta[1],
											state.pendingCameraTranslationDelta[2],
											cameraEvaluation, weight);
									}
								} else {
									static std::atomic<bool> s_invalidCameraModelWarned{ false };
									if (!s_invalidCameraModelWarned.exchange(true)) {
										logger::warn(
											"[OAR-TrackFilter-Camera] Model-space result for '{}' was invalid; "
											"leaving Camera native",
											state.suffix);
									}
								}
							} else if (!haveModels) {
								static std::atomic<bool> s_cameraChainWarned{ false };
								if (!s_cameraChainWarned.exchange(true)) {
									logger::warn(
										"[OAR-TrackFilter-Camera] Could not reconstruct the Camera model-space chain "
										"for '{}'; leaving Camera native",
										state.suffix);
								}
							}
						}
						continue;
					}

					// Chain roots get the anchored local (see setup above). The
					// walk only touches ancestors OUTSIDE the filter set, which the
					// loop never overwrites, so iteration order doesn't matter.
					if (doAnchor && idx < skelBoneCount) {
						const int16_t par = skelParents[idx];
						if (par < 0 || !tl_filteredSet.count(par)) {
							RE::hkQsTransformRaw donorM, parentM;
							if (modelTransform(idx, &tl_sampledTracks, donorM)) {
								if (par >= 0 && modelTransform(par, nullptr, parentM)) {
									InverseQs(parentM, parentM);
									ComposeQs(parentM, donorM, repVal);
								} else if (par < 0) {
									repVal = donorM;
								}
							}
						}
					}

					state.cachedRepByName[name] = repVal;
					state.cachedBaseByName[name] = baseVal;

					// At ~zero weight this pass only primes the cache (blend-in's
					// first frame): don't touch the pose or set mask bits.
					if (weight > 0.001f) {
						if (mode == SubMod::TrackFilter::Mode::Override) {
							LerpTransform(outputPose[idx], repVal, weight);
						} else {
							BlendAdditiveTransform(outputPose[idx], baseVal, repVal, weight);
						}
						// Mark this bone as MODIFIED in the pose's bone mask, so the
						// engine's downstream pose composition honors our write.
						SetPoseBoneMaskBit(tracksPtr, poseHeader, idx);
					}
				}
				if (wantBindingDiag) s_bindingDiagLog++;

				// Frozen bones: neither the donor nor the native animation drives
				// them. Capture the native local on the first sample of this play
				// (outputPose still holds the source clip's own value here - the
				// freeze set is disjoint from the donor set), then hold it at the
				// overlay's weight so blend-in/out release it smoothly.
				for (auto& [name, idx] : cr.freezeNameAndIndex) {
					if (IsTrackFilterCameraBone(name)) continue;
					if (idx < 0 || idx >= numOutputBones) continue;
					auto fIt = state.frozenByName.find(name);
					if (fIt == state.frozenByName.end()) {
						fIt = state.frozenByName.emplace(name, outputPose[idx]).first;
					}
					if (weight > 0.001f) {
						LerpTransform(outputPose[idx], fIt->second, weight);
						SetPoseBoneMaskBit(tracksPtr, poseHeader, idx);
					}
				}

				// onFraction is > 0 here (source path has an early-out at the top).

				state.cacheValid = true;
				state.lastSourceTimeSec = nowSec;
				state.lastSampleSec = nowSec;
				if (state.standaloneSpecialIdle) {
					state.lastStandaloneSampleFrame = currentFrame;
				}

				static int s_finalLog = 0;
				if (s_finalLog < 5) {
					int dumped = 0;
					for (auto& [name, idx] : cr.nameAndIndex) {
						if (dumped++ >= 2) break;
						if (idx < 0 || idx >= numOutputBones) continue;
						auto& f = outputPose[idx];
						auto baseIt = state.cachedBaseByName.find(name);
						auto repIt = state.cachedRepByName.find(name);
						if (baseIt == state.cachedBaseByName.end() || repIt == state.cachedRepByName.end())
							continue;
						logger::info("[OAR-TrackFilter] FINAL(direct) src char={:X} '{}'[{}]: base=({:.3f},{:.3f},{:.3f}) rep=({:.3f},{:.3f},{:.3f}) out=({:.3f},{:.3f},{:.3f}) mode={} w={:.2f}",
							reinterpret_cast<uintptr_t>(character), name, idx,
							baseIt->second.translation[0], baseIt->second.translation[1], baseIt->second.translation[2],
							repIt->second.translation[0], repIt->second.translation[1], repIt->second.translation[2],
							f.translation[0], f.translation[1], f.translation[2],
							mode == SubMod::TrackFilter::Mode::Override ? "Override" : "Additive",
							weight);
					}
					s_finalLog++;
				}
				return;
			}

			// ============== Swap-and-Generate fallback ==============
			// Swap the binding's animation pointer to our replacement, call
			// _Generate to let the engine sample it through its normal code path
			// (which handles null offsets in the clone), then read the result.
			// NOTE: this path samples at the clip's own playback time, so the
			// fixed-frame option cannot be honored here. Warn once so the user
			// knows why the pose follows playback instead of holding a frame.
			if (filterPtr->sampleFrame >= 0.0f) {
				static std::atomic<bool> s_fixedFrameFallbackWarned{ false };
				if (!s_fixedFrameFallbackWarned.exchange(true)) {
					logger::warn("[OAR-TrackFilter] SubMod '{}' requests fixed-frame sampling (frame {:.0f}) "
						"but this clip has no readable track binding — falling back to playback-time sampling.",
						state.parentSubMod ? state.parentSubMod->GetName() : "?", filterPtr->sampleFrame);
				}
			}
			if (!animSlot || !*animSlot) return;
			// This path swaps and recursively generates — there is no cache-only
			// priming mode, so at ~zero weight just skip it entirely.
			if (weight <= 0.001f) return;

			RE::hkaAnimation* originalInSlot = *animSlot;

			thread_local std::vector<RE::hkQsTransformRaw> tl_fullBasePose;
			tl_fullBasePose.resize(numOutputBones);
			memcpy(tl_fullBasePose.data(), outputPose, numOutputBones * sizeof(RE::hkQsTransformRaw));

			*animSlot = replacement;
			tfLock.unlock();
			Hooks::ClipGeneratorHooks::_Generate(a_this, a_context, a_activeChildrenOutput, a_output, a_timeOffset);
			tfLock.lock();

			*animSlot = originalInSlot;

			// Re-find EVERYTHING after the unlock window: other threads may have
			// erased this state or reallocated the actor's state vector (which
			// invalidates both `state` and `cr` references captured above).
			auto* statePtr2 = FindTrackFilterState(actor, filterKey);
			if (!statePtr2) {
				memcpy(outputPose, tl_fullBasePose.data(), numOutputBones * sizeof(RE::hkQsTransformRaw));
				return;
			}
			auto& state2 = *statePtr2;
			auto& cr2 = state2.resolvedByChar[character];

			for (auto& [name, idx] : cr2.nameAndIndex) {
				if (idx < 0 || idx >= numOutputBones) continue;
				if (IsTrackFilterCameraBone(name)) {
					static std::atomic<bool> s_cameraFallbackWarned{ false };
					if (!s_cameraFallbackWarned.exchange(true)) {
						logger::warn(
							"[OAR-TrackFilter-Camera] '{}' has no reliable direct donor binding; "
							"leaving Camera native instead of applying an absolute fallback pose",
							state2.suffix);
					}
					continue;
				}
				state2.cachedRepByName[name] = outputPose[idx];
				state2.cachedBaseByName[name] = tl_fullBasePose[idx];
			}
			state2.cacheValid = true;
			state2.lastSourceTimeSec = nowSec;
			state2.lastSampleSec = nowSec;
			if (state2.standaloneSpecialIdle) {
				state2.lastStandaloneSampleFrame = currentFrame;
			}

			memcpy(outputPose, tl_fullBasePose.data(), numOutputBones * sizeof(RE::hkQsTransformRaw));

			for (auto& [name, idx] : cr2.nameAndIndex) {
				if (idx < 0 || idx >= numOutputBones) continue;
				if (IsTrackFilterCameraBone(name)) {
					if (state2.blendingOut || state2.dormant) {
						SetPoseBoneMaskBit(tracksPtr, poseHeader, idx);
					}
					continue;
				}
				auto rIt = state2.cachedRepByName.find(name);
				if (rIt == state2.cachedRepByName.end()) continue;
				if (mode == SubMod::TrackFilter::Mode::Override) {
					LerpTransform(outputPose[idx], rIt->second, weight);
				} else {
					auto bIt = state2.cachedBaseByName.find(name);
					if (bIt != state2.cachedBaseByName.end())
						BlendAdditiveTransform(outputPose[idx], bIt->second, rIt->second, weight);
				}
				SetPoseBoneMaskBit(tracksPtr, poseHeader, idx);
			}

			// Frozen bones (see the direct path): capture from the restored base
			// pose (the clip's own native locals), then hold.
			for (auto& [name, idx] : cr2.freezeNameAndIndex) {
				if (IsTrackFilterCameraBone(name)) continue;
				if (idx < 0 || idx >= numOutputBones) continue;
				auto fIt = state2.frozenByName.find(name);
				if (fIt == state2.frozenByName.end()) {
					fIt = state2.frozenByName.emplace(name, outputPose[idx]).first;
				}
				LerpTransform(outputPose[idx], fIt->second, weight);
				SetPoseBoneMaskBit(tracksPtr, poseHeader, idx);
			}
			// onFraction is > 0 here (source path has an early-out at the top).

			static int s_fbFinalLog = 0;
			if (s_fbFinalLog < 10) {
				for (auto& [name, idx] : cr2.nameAndIndex) {
					if (idx < 0 || idx >= numOutputBones) continue;
					auto& f = outputPose[idx];
					auto bIt = state2.cachedBaseByName.find(name);
					auto rIt = state2.cachedRepByName.find(name);
					if (bIt != state2.cachedBaseByName.end() && rIt != state2.cachedRepByName.end()) {
						logger::info("[OAR-TrackFilter] FINAL(fallback) '{}'[{}]: base=({:.3f},{:.3f},{:.3f}) rep=({:.3f},{:.3f},{:.3f}) out=({:.3f},{:.3f},{:.3f}) mode={} w={:.2f}",
							name, idx,
							bIt->second.translation[0], bIt->second.translation[1], bIt->second.translation[2],
							rIt->second.translation[0], rIt->second.translation[1], rIt->second.translation[2],
							f.translation[0], f.translation[1], f.translation[2],
							mode == SubMod::TrackFilter::Mode::Override ? "Override" : "Additive",
							weight);
					}
					break;
				}
				s_fbFinalLog++;
			}
			return;
		}

		// =====================================================================
		// NON-SOURCE CLIP PATH (slow): same logic as the fast path but reached
		// when bones weren't yet resolved or cache wasn't valid above.
		// =====================================================================
		if (!state.cacheValid) return;
		if (ShouldSkipAddNonSourceClip(a_this, filterPtr)) return;
		// Skip inactive clips — their pose buffer is uninitialized
		if (poseHeader.onFraction <= 0.f) return;

		// Same policy as fast path: override everything except sneak-related clips.
		{
			std::string clipSuffix;
			{
				std::shared_lock csLock(s_clipSuffixMutex);
				auto csIt = s_clipSuffixCache.find(a_this);
				if (csIt != s_clipSuffixCache.end())
					clipSuffix = csIt->second;
			}
			if (!clipSuffix.empty()) {
				auto leafView = GetSuffixLeaf(clipSuffix);
				if (leafView.size() >= 5 && leafView.substr(0, 5) == "sneak") return;
			}
		}

		const bool isAdditiveClip = (poseHeader.flags & 0x01) != 0;

		for (auto& [name, idx] : cr.nameAndIndex) {
			if (IsTrackFilterCameraBone(name)) continue;
			if (idx < 0 || idx >= numOutputBones) continue;

			if (isAdditiveClip) {
				RE::hkQsTransformRaw identity{};
				identity.translation[0] = 0.f; identity.translation[1] = 0.f;
				identity.translation[2] = 0.f; identity.translation[3] = 0.f;
				identity.rotation[0] = 0.f; identity.rotation[1] = 0.f;
				identity.rotation[2] = 0.f; identity.rotation[3] = 1.f;
				identity.scale[0] = 1.f; identity.scale[1] = 1.f;
				identity.scale[2] = 1.f; identity.scale[3] = 0.f;
				outputPose[idx] = identity;
			} else {
				auto rIt = state.cachedRepByName.find(name);
				if (rIt == state.cachedRepByName.end()) continue;
				if (mode == SubMod::TrackFilter::Mode::Override) {
					LerpTransform(outputPose[idx], rIt->second, weight);
				} else {
					auto bIt = state.cachedBaseByName.find(name);
					if (bIt != state.cachedBaseByName.end()) {
						BlendAdditiveTransform(outputPose[idx], bIt->second, rIt->second, weight);
					} else {
						LerpTransform(outputPose[idx], rIt->second, weight);
					}
				}
			}
			SetPoseBoneMaskBit(tracksPtr, poseHeader, idx);
		}

		// Frozen bones (see the fast path).
		for (auto& [name, idx] : cr.freezeNameAndIndex) {
			if (IsTrackFilterCameraBone(name)) continue;
			if (idx < 0 || idx >= numOutputBones) continue;
			if (isAdditiveClip) {
				RE::hkQsTransformRaw identity{};
				identity.translation[0] = 0.f; identity.translation[1] = 0.f;
				identity.translation[2] = 0.f; identity.translation[3] = 0.f;
				identity.rotation[0] = 0.f; identity.rotation[1] = 0.f;
				identity.rotation[2] = 0.f; identity.rotation[3] = 1.f;
				identity.scale[0] = 1.f; identity.scale[1] = 1.f;
				identity.scale[2] = 1.f; identity.scale[3] = 0.f;
				outputPose[idx] = identity;
			} else {
				auto fIt = state.frozenByName.find(name);
				if (fIt == state.frozenByName.end()) continue;
				LerpTransform(outputPose[idx], fIt->second, weight);
			}
			SetPoseBoneMaskBit(tracksPtr, poseHeader, idx);
		}

		static int s_nonSrcFinalLog = 0;
		if (s_nonSrcFinalLog < 5) {
			int dumped = 0;
			for (auto& [name, idx] : cr.nameAndIndex) {
				if (dumped++ >= 2) break;
				if (idx < 0 || idx >= numOutputBones) continue;
				auto& f = outputPose[idx];
				logger::info("[OAR-TrackFilter] FINAL nonsrc(slow) '{}'[{}]: trans=({:.3f},{:.3f},{:.3f}) additive={}",
					name, idx, f.translation[0], f.translation[1], f.translation[2], isAdditiveClip);
			}
			s_nonSrcFinalLog++;
		}
		}; // end processFilter lambda

		for (auto* filterKey : actorFilters) {
			processFilter(filterKey);
		}
	}

	void hkbClipGenerator_StartEcho(RE::hkbClipGenerator* a_this, float a_duration)
	{
		// StartEcho re-enters a clip WITHOUT Deactivate or Activate. A shared
		// binding can therefore still contain a clone retired by a weapon-change
		// invalidation, and the engine captures that stale animation before the
		// next Update gets a chance to re-evaluate the now-false conditions.
		// Restore only RETIRED clones here. activeSubMod=false is not sufficient:
		// field logs also show legitimate, still-live clones entering StartEcho
		// through a different generator while the original weapon remains active.
		if (a_this && s_gameFullyLoaded.load()) {
			if (auto** echoSlot = a_this->GetAnimationSlot(); echoSlot && *echoSlot) {
				auto* cache = AnimationCache::GetSingleton();
				auto* retiredClone = *echoSlot;
				auto* original = cache->GetOriginalFromRetiredReplacement(retiredClone);
				if (original && !IsBadReadPtr(original, sizeof(uintptr_t)) &&
					IsPlausibleGameAnimVtable(*reinterpret_cast<uintptr_t*>(original))) {
					*echoSlot = original;
					RestoreClipTriggers(a_this);
					static std::atomic<int> s_echoRetiredRestoreLog{ 0 };
					if (s_echoRetiredRestoreLog.fetch_add(1, std::memory_order_relaxed) < 30) {
						logger::info("[OAR] StartEcho: restored retired clone before echo (clipGen={:X}, clone={:X}, original={:X})",
							reinterpret_cast<uintptr_t>(a_this), reinterpret_cast<uintptr_t>(retiredClone),
							reinterpret_cast<uintptr_t>(original));
					}
				} else if (cache->IsOurReplacement(retiredClone)) {
					bool echoActive = false;
					{
						std::shared_lock smLock(s_activeSubModMutex);
						echoActive = s_activeSubModMap.find(a_this) != s_activeSubModMap.end();
					}
					static std::atomic<int> s_echoCloneDiagLog{ 0 };
					if (s_echoCloneDiagLog.fetch_add(1, std::memory_order_relaxed) < 30) {
						logger::info("[OAR] StartEcho: clone remains in slot at echo entry (clipGen={:X}, activeSubMod={})",
							reinterpret_cast<uintptr_t>(a_this), echoActive);
					}
				}
			}
		}

		Hooks::ClipGeneratorHooks::_StartEcho(a_this, a_duration);

		// Signal that an echo event occurred — non-interruptible submods with
		// replaceOnEcho=true will re-evaluate conditions once on the next Update.
		if (a_this) {
			std::unique_lock leLock(s_loopEchoFlagMutex);
			s_clipEchoPending[a_this] = true;
		}

		// An echo restarts the clip's time from ~0 WITHOUT a Deactivate/Activate
		// cycle. If a replacement is active and the completion-restore already
		// put the ORIGINAL triggers back (single-play clips), the engine would
		// natively fire the original's t~0 annotations (WeaponFire!) on the
		// replay. Re-NULL before the next Update processes the restarted time.
		if (a_this) {
			SubMod* echoActiveSub = nullptr;
			bool replacementActive = false;
			{
				std::shared_lock smLock(s_activeSubModMutex);
				auto smIt = s_activeSubModMap.find(a_this);
				if (smIt != s_activeSubModMap.end()) echoActiveSub = smIt->second;
				replacementActive = (echoActiveSub && echoActiveSub->GetReplaceAnnotations());
			}
			RearmIdleStopSuppressionForEcho(a_this, echoActiveSub);
			if (replacementActive) {
				bool wasRestored = false;
				{
					std::lock_guard rg(s_triggersRestoredMutex);
					wasRestored = s_triggersRestoredSet.erase(a_this) > 0;
				}
				if (wasRestored) {
					// This rebuild wins the race against the Update hook's
					// re-assert (backup.nulled blocks later rebuilds), so the End
					// Clip If Shorter re-timing must be decided here too. The
					// replacement is whatever OAR clone sits in the slot.
					RE::hkaAnimation* echoRep = nullptr;
					if (auto** echoSlot = a_this->GetAnimationSlot(); echoSlot && *echoSlot &&
						AnimationCache::GetSingleton()->IsOurReplacement(*echoSlot)) {
						echoRep = *echoSlot;
					}
					const float endClipDurEcho = EndClipIfShorterDuration(
						echoActiveSub, a_this, echoRep, GetValidOriginal(a_this));
					InstallReplacementTriggers(a_this, "", endClipDurEcho);
					static int s_echoRenullLog = 0;
					if (s_echoRenullLog < 20) {
						logger::info("[OAR-Triggers] Echo restart — re-NULL'd triggers for clipGen={:X}",
							reinterpret_cast<uintptr_t>(a_this));
						s_echoRenullLog++;
					}
				}
			}
		}
	}

	void HookedActorUpdate()
	{
		s_currentFrame.fetch_add(1, std::memory_order_relaxed);

		// Process the original actor updates FIRST so game state is current.
		// Null on NG/AE: there the per-frame driver is F4SE's permanent task
		// queue (see UpdateHooks::Install), which runs alongside the game's
		// own actor updates rather than wrapping them.
		if (Hooks::UpdateHooks::RunActorUpdatesOrig) {
			Hooks::UpdateHooks::RunActorUpdatesOrig();
		}

	// Track-filter bone debug aids (green mesh highlight + 3D name labels).
	// Runs here because this is the game thread with the scene graph in a
	// stable state; no-ops in a single check while the feature is unused.
	BoneDebugViz::OnGameTick();

	// Per-frame aim-state stamp for the IsADS condition's grace window
	// (reloads drop sighted state on the same frame the reload clip starts,
	// so conditions need last-sighted history rather than a live probe).
	ConditionTracking::TickPlayerAimState();

		// The Settings "Enabled" box was just unticked on the UI thread:
		// perform the vanilla restore here on the game thread, outside the
		// Havok update cycle (see OnGlobalEnabledChanged).
		if (s_pendingGlobalDisableRestore.exchange(false)) {
			PerformGlobalDisableRestore();
		}

		// "Reload All Configs" clicked in the UI: run the teardown + re-parse
		// here, serialized with graph updates (see PerformConfigReload).
		if (s_pendingConfigReload.exchange(false)) {
			try {
				PerformConfigReload();
			} catch (const std::exception& e) {
				logger::error("[OAR] Config reload failed: {}", e.what());
			}
		}

		// A submod priority was edited in the UI: re-sort the candidate
		// vectors so the new order applies immediately.
		if (s_pendingLookupResort.exchange(false)) {
			ResortNameLookupByPriority();
		}

		// Fire deferred custom events now that the Havok update cycle is complete.
		FlushDeferredEvents();

		// Enumerate the player's active clips and resolve their real animation
		// paths — GunMover's per-frame model; must run OUTSIDE the Havok update
		// cycle (which has just completed) so activeNodes is stable.
		if (s_gameFullyLoaded.load() && s_lookupBuilt) {
			PollPlayerGraphClips();

			// Register every loaded (high-process) actor's graph characters in
			// the character->refr cache so GetRefrFromContext can attribute
			// NPC/creature clips in the Animation Log. Without this, only the
			// player was ever registered and every other actor showed as
			// "Unknown (0x00000000)". Every 30 frames (~0.5s) is fresh enough:
			// an actor's graphs persist for its lifetime, so at worst a newly
			// spawned actor's first few entries are unattributed.
			static uint64_t s_lastActorRegisterFrame = 0;
			const auto curFrame = s_currentFrame.load(std::memory_order_relaxed);
			if (curFrame - s_lastActorRegisterFrame >= 30) {
				s_lastActorRegisterFrame = curFrame;
				if (auto* processLists = OAR_RE::ProcessLists::GetSingleton()) {
					for (auto& handle : processLists->highActorHandles) {
						if (auto actorPtr = handle.get()) {
							RegisterActorCharacter(actorPtr.get());
						}
					}
				}
				RegisterActorCharacter(RE::PlayerCharacter::GetSingleton());
			}
		}

		// Auto-reload replication: last-round / fire-empty detection and the
		// delayed DoAction(kActionReload) trigger. Needs current game state,
		// so it runs after the actor updates above.
		if (s_gameFullyLoaded.load()) {
			Hooks::AutoReloadSuppression::PerFrameUpdate();
		}

		// Compute frame delta time for blend ramping (shared by track filter + full-body)
		static const auto s_initTick = std::chrono::high_resolution_clock::now();
		static auto s_lastTick = s_initTick;
		auto now = std::chrono::high_resolution_clock::now();
		float dt = std::chrono::duration<float>(now - s_lastTick).count();
		dt = std::clamp(dt, 0.0001f, 0.1f);
		s_lastTick = now;

		// Publish wall-clock "now" for the Generate hook's staleness stamps.
		s_tfNowSec.store(std::chrono::duration<float>(now - s_initTick).count(),
			std::memory_order_relaxed);

		// --- Track filter blend update ---
		if (s_trackFilterActiveCount.load(std::memory_order_relaxed) > 0) {
			struct PendingEval {
				RE::TESObjectREFR* actor;
				const SubMod::TrackFilter* filter;  // identifies the state within the actor
				SubMod* subMod;
				std::string suffix;
				RE::hkbClipGenerator* sourceClip;
			};
			std::vector<PendingEval> toEval;
			{
				std::shared_lock tfShared(s_trackFilterMutex);
				toEval.reserve(s_charTrackFilterMap.size());
				for (auto& [actor, states] : s_charTrackFilterMap) {
					for (auto& state : states) {
						if (state.parentSubMod && actor)
							toEval.push_back({ actor, state.filter, state.parentSubMod, state.suffix, state.sourceClip });
					}
				}
			}

			// Conditions are evaluated per (actor, filter) pair — multiple filters
			// can be active on one actor and each deactivates independently.
			std::set<std::pair<RE::TESObjectREFR*, const SubMod::TrackFilter*>> conditionsFalse;
			for (auto& pe : toEval) {
				if (pe.subMod->GetPlayOnceFullBody()) continue;
				// Non-interruptible PLAYBACK-FOLLOWING overlays honor the same
				// contract as non-interruptible full-body replacements: conditions
				// gate activation, then the play rides through to the donor's end.
				// Their termination comes from the one-shot path (donor completed /
				// source stopped sampling), which is why re-evaluating conditions
				// here cut a grenade throw at the release frame — IsAttacking drops
				// a few tenths before the animation actually finishes (2026-08-04).
				//
				// FIXED-FRAME pose holders (sampleFrame >= 0) always re-evaluate:
				// a condition flip is their ONLY end signal, and blendOutTime (not
				// the interruptible flag) provides the smooth exit.
				if (!pe.subMod->IsInterruptible() && pe.filter && pe.filter->sampleFrame < 0.0f)
					continue;
				if (!pe.subMod->EvaluateConditions(pe.actor, pe.sourceClip))
					conditionsFalse.insert({ pe.actor, pe.filter });
			}

			{
				const float nowSec = s_tfNowSec.load(std::memory_order_relaxed);
				std::unique_lock tfLock(s_trackFilterMutex);
				for (auto mapIt = s_charTrackFilterMap.begin(); mapIt != s_charTrackFilterMap.end(); ) {
					auto& states = mapIt->second;
					for (auto stIt = states.begin(); stIt != states.end(); ) {
						auto& state = *stIt;
						auto* filterPtr = state.filter;
						bool condFalse = conditionsFalse.count({ mapIt->first, filterPtr }) > 0;

						// Dormant: applies nothing (alpha 0), kept while its source clip
						// is alive so revival never goes through erase-and-recreate
						// (a fresh state per frame was the register/erase churn,
						// 2026-08-04 01:53 session). Erased ONLY by staleness — the
						// clip generator actually going away. NOT erased on condFalse:
						// for non-interruptible submods the Update hook re-registers
						// the winner every frame regardless of conditions, so a
						// condFalse erase just recreates the state next frame.
						if (state.dormant) {
							// Wake rules by dormancy reason:
							//  - one-shot completed: only a fresh play (restart
							//    detection / registration) wakes it, handled elsewhere.
							//    earlyBlendOutArmed counts as completed even though
							//    oneShotDone is still false — an end-anchored fade
							//    finishes a frame or two BEFORE the donor's final
							//    frame, and without this guard conditions still being
							//    true woke the overlay right back up and it flapped
							//    against the re-arming fade at the end of every play.
							//  - sample-starved: samples resuming wake it, handled in
							//    the sampling path.
							//  - condition-ended: conditions turning true again wake
							//    it here (persistent-overlay semantics).
							if (!condFalse && !state.oneShotDone && !state.earlyBlendOutArmed &&
								!state.sampleStarved) {
								state.dormant = false;
								state.blendingOut = false;
								float blendIn = filterPtr ? filterPtr->blendInTime : 0.0f;
								state.blendDuration = blendIn;
								state.blendElapsed = 0.0f;
								state.blendAlpha = (blendIn <= 0.0f) ? 1.0f : 0.0f;
								if (state.onEndFired && state.parentSubMod) {
									state.onEndFired = false;
									QueueCustomEvents(mapIt->first, state.parentSubMod->eventsOnStart, "onStart/trackFilter-restart");
								}
								++stIt;
								continue;
							}
							if (nowSec - state.lastSourceTimeSec > kTrackFilterStaleSeconds) {
								if (!state.onEndFired && state.parentSubMod) {
									QueueCustomEvents(mapIt->first, state.parentSubMod->eventsOnEnd, "onEnd/trackFilter");
								}
								stIt = states.erase(stIt);
								s_trackFilterActiveCount.fetch_sub(1, std::memory_order_relaxed);
								continue;
							}
							++stIt;
							continue;
						}

						// Staleness cleanup: if all source clips are gone and no source
						// has generated for kTrackFilterStaleSeconds (wall-clock — frame
						// counts would shrink the window at high framerates), treat as
						// condition-false to start blend-out. This handles the case where
						// source clips deactivate and conditions are non-interruptible
						// (never re-evaluated).
						if (!condFalse && state.sourceClips.empty() && !state.blendingOut &&
							nowSec - state.lastSourceTimeSec > kTrackFilterStaleSeconds) {
							condFalse = true;
						}

						// One-shot end for playback-following filters (sampleFrame < 0):
						// the overlay ends itself when the donor has played through
						// (oneShotDone, set by the sampling path) or when the source
						// stops producing samples (clip went zero-weight / ended before
						// the donor). Conditions staying true must NOT keep the overlay
						// pinned — that froze a grenade-throw arm at its final frame
						// indefinitely (2026-08-04 session). Fixed-frame pose donors
						// keep the persistent-overlay semantics and are untouched.
						bool oneShotEnd = false;
						if (filterPtr && filterPtr->sampleFrame < 0.0f &&
							!state.blendingOut && !condFalse) {
							if (state.oneShotDone || state.earlyBlendOutArmed) {
								oneShotEnd = true;
							} else if (state.lastSampleSec > 0.0f &&
								nowSec - state.lastSampleSec > kOneShotSampleGraceSeconds) {
								state.sampleStarved = true;
								oneShotEnd = true;
							}
						}

						if (oneShotEnd) {
							// No deactivation delay here — that setting is for
							// condition-driven ends; a finished one-shot goes now.
							state.deactivationDelayActive = false;
							state.blendingOut = true;
							state.blendDuration = filterPtr ? filterPtr->blendOutTime : 0.0f;
							// Continue from the current alpha (alpha maps to elapsed
							// via the inverse of the ease, approximated linearly).
							// alpha = 1 - curve(t), so resuming at the current alpha means
							// t = curve⁻¹(1 - alpha). The old linear form only matched
							// the ramp when the curve was linear.
							state.blendElapsed = InverseBlendCurve(CurveOf(state.parentSubMod),
								1.0f - state.blendAlpha) * state.blendDuration;
							static int s_osLog = 0;
							if (s_osLog < 10) {
								logger::info("[OAR-TrackFilter] One-shot end for '{}' ({}) — blending out ({:.2f}s)",
									state.suffix,
									state.oneShotDone ? "donor completed"
										: state.earlyBlendOutArmed ? "approaching donor end"
										: "source stopped sampling",
									state.blendDuration);
								s_osLog++;
							}
						} else if (condFalse && !state.blendingOut) {
							float deactivDelay = state.parentSubMod ? state.parentSubMod->GetDeactivationDelay() : 0.0f;
							if (deactivDelay > 0.0f && !state.deactivationDelayActive) {
								state.deactivationDelayActive = true;
								state.deactivationDelayRemaining = deactivDelay;
							}

							if (!state.deactivationDelayActive || state.deactivationDelayRemaining <= 0.0f) {
								state.deactivationDelayActive = false;
								state.blendingOut = true;
								state.blendDuration = filterPtr ? filterPtr->blendOutTime : 0.0f;
								// Start from the current alpha, not from 1: restarting
								// a blend-out that was already in progress used to snap
								// alpha back to full (part of the 9-restart flap).
								// alpha = 1 - curve(t), so resuming at the current alpha means
							// t = curve⁻¹(1 - alpha). The old linear form only matched
							// the ramp when the curve was linear.
							state.blendElapsed = InverseBlendCurve(CurveOf(state.parentSubMod),
								1.0f - state.blendAlpha) * state.blendDuration;
								static int s_boLog = 0;
								if (s_boLog < 10) {
									logger::info("[OAR-TrackFilter] Blend-out started for '{}' (duration={:.2f}s)",
										state.suffix, state.blendDuration);
									s_boLog++;
								}
							}
						} else if (!condFalse) {
							state.deactivationDelayActive = false;
							state.deactivationDelayRemaining = 0.0f;
							// Conditions true again during a CONDITION-driven blend-out:
							// blend back in from the current alpha. One-shot/starvation
							// blend-outs ignore conditions entirely. This (not the old
							// unconditional cancel at re-registration) is now the single
							// authority for condition-driven reactivation.
							//
							// earlyBlendOutArmed belongs to the "ignore conditions" set
							// too: an end-anchored fade starts while the donor is still
							// playing, so oneShotDone is still false and the grenade
							// throw's conditions are still true. Without it this branch
							// cancelled the fade on the very next tick, the arm branch
							// re-armed it the tick after, and the pair ping-ponged at
							// full alpha for the donor's whole tail — then the donor hit
							// its end, clamped, and held that final pose through a fade
							// that only started AFTER the animation was over. Exactly
							// the symptom the feature was meant to remove.
							if (state.blendingOut && !state.oneShotDone && !state.earlyBlendOutArmed &&
								!state.sampleStarved) {
								state.blendingOut = false;
								float blendIn = filterPtr ? filterPtr->blendInTime : 0.0f;
								state.blendDuration = blendIn;
								state.blendElapsed = (blendIn > 0.0f)
	? InverseBlendCurve(CurveOf(state.parentSubMod), state.blendAlpha) * blendIn : 0.0f;
								if (blendIn <= 0.0f) state.blendAlpha = 1.0f;
							}
						}

						if (state.deactivationDelayActive) {
							state.deactivationDelayRemaining -= dt;
						}

						if (state.blendingOut) {
							// A finished blend-out ALWAYS parks the state dormant.
							// Erasing here while the source clip is still alive made
							// the Update hook re-register a fresh state the very next
							// frame (per-frame churn); the dormant branch above owns
							// the actual erase via staleness. Custom "on end" events
							// fire here — the visible end of the overlay — guarded so
							// a later erase doesn't fire them again.
							auto goDormant = [&]() {
								state.dormant = true;
								state.blendingOut = false;
								state.blendAlpha = 0.0f;
								if (!state.onEndFired && state.parentSubMod) {
									QueueCustomEvents(mapIt->first, state.parentSubMod->eventsOnEnd, "onEnd/trackFilter");
									state.onEndFired = true;
								}
							};

							if (state.blendDuration <= 0.0f) {
								goDormant();
								++stIt;
								continue;
							}
							state.blendElapsed += dt;
							float t = std::clamp(state.blendElapsed / state.blendDuration, 0.0f, 1.0f);
							state.blendAlpha = 1.0f - EvaluateBlendCurve(CurveOf(state.parentSubMod), t);

							// Diagnostic: log blend-out progress every ~0.1s
							static float s_lastBlendLog = 0.0f;
							if (state.blendElapsed - s_lastBlendLog > 0.1f || state.blendAlpha <= 0.001f) {
								logger::info("[OAR-TrackFilter] BLEND-OUT tick: suffix='{}' elapsed={:.3f}/{:.3f} t={:.3f} alpha={:.4f} dt={:.4f}",
									state.suffix, state.blendElapsed, state.blendDuration, t, state.blendAlpha, dt);
								s_lastBlendLog = state.blendElapsed;
							}

							if (state.blendAlpha <= 0.001f) {
								s_lastBlendLog = 0.0f;
								static int s_dormLog = 0;
								if (s_dormLog < 10) {
									logger::info("[OAR-TrackFilter] Blend-out COMPLETE — '{}' dormant (awaiting reactivation)", state.suffix);
									s_dormLog++;
								}
								goDormant();
								++stIt;
								continue;
							}
						} else {
							float blendInTime = filterPtr ? filterPtr->blendInTime : 0.0f;
							if (blendInTime <= 0.0f || state.blendAlpha >= 1.0f) {
								state.blendAlpha = 1.0f;
							} else {
								state.blendElapsed += dt;
								float t = std::clamp(state.blendElapsed / blendInTime, 0.0f, 1.0f);
								state.blendAlpha = EvaluateBlendCurve(CurveOf(state.parentSubMod), t);
							}
						}
						++stIt;
					}

					// Drop the actor entry once its last filter state is gone.
					if (states.empty()) {
						mapIt = s_charTrackFilterMap.erase(mapIt);
					} else {
						++mapIt;
					}
				}
			}
		}

		// --- Full-body replacement blend update ---
		if (s_fullBodyBlendActiveCount.load(std::memory_order_relaxed) > 0) {
			std::unique_lock fbLock(s_fullBodyBlendMutex);
			for (auto it = s_fullBodyBlendMap.begin(); it != s_fullBodyBlendMap.end(); ) {
				auto& bs = it->second;
				if (bs.blendingIn) {
					if (bs.blendDuration <= 0.0f) {
						bs.blendAlpha = 1.0f;
						bs.blendingIn = false;
					} else {
						bs.blendElapsed += dt;
						float t = std::clamp(bs.blendElapsed / bs.blendDuration, 0.0f, 1.0f);
						bs.blendAlpha = EvaluateBlendCurve(bs.blendCurve, t);
						if (bs.blendAlpha >= 0.999f) {
							bs.blendAlpha = 1.0f;
							bs.blendingIn = false;
						}
					}
					++it;
				} else if (bs.blendingOut) {
					if (bs.blendDuration <= 0.0f) {
						bs.blendAlpha = 0.0f;
					} else {
						bs.blendElapsed += dt;
						float t = std::clamp(bs.blendElapsed / bs.blendDuration, 0.0f, 1.0f);
						bs.blendAlpha = 1.0f - EvaluateBlendCurve(bs.blendCurve, t);
					}
					if (bs.blendAlpha <= 0.001f) {
						it = s_fullBodyBlendMap.erase(it);
						s_fullBodyBlendActiveCount.fetch_sub(1, std::memory_order_relaxed);
						continue;
					}
					++it;
				} else {
					++it;
				}
			}
		}
	}

	RE::hkbCharacterStringData* ExtractStringDataFromGraph(void* a_firstArg)
	{
		auto* graphBytes = reinterpret_cast<uint8_t*>(a_firstArg);

		auto* charPtr = reinterpret_cast<RE::hkbCharacter*>(graphBytes + 0x1C8);
		if (IsBadReadPtr(charPtr, sizeof(void*))) {
			logger::warn("[OAR]   ExtractSD: character at graph+0x1C8 unreadable");
			return nullptr;
		}

		auto* setup = charPtr->setup._ptr;
		if (!setup) {
			logger::info("[OAR]   ExtractSD: character.setup is null (graph may be initializing)");
			return nullptr;
		}
		if (IsBadReadPtr(setup, sizeof(void*))) {
			logger::warn("[OAR]   ExtractSD: character.setup unreadable");
			return nullptr;
		}

		auto* typedSetup = reinterpret_cast<RE::hkbCharacterSetup*>(setup);
		if (!typedSetup->data._ptr) {
			logger::info("[OAR]   ExtractSD: setup.data is null");
			return nullptr;
		}
		if (IsBadReadPtr(typedSetup->data._ptr, sizeof(void*))) {
			logger::warn("[OAR]   ExtractSD: setup.data unreadable");
			return nullptr;
		}

		auto* stringData = typedSetup->data._ptr->stringData._ptr;
		if (!stringData) {
			logger::info("[OAR]   ExtractSD: data.stringData is null");
			return nullptr;
		}
		if (IsBadReadPtr(stringData, sizeof(uintptr_t))) {
			logger::warn("[OAR]   ExtractSD: stringData unreadable");
			return nullptr;
		}

		uintptr_t sdVtbl = *reinterpret_cast<uintptr_t*>(stringData);
		if (sdVtbl != Offsets::hkbCharacterStringData_vtbl.address()) {
			logger::warn("[OAR]   ExtractSD: stringData vtable mismatch ({:X} vs {:X})",
				sdVtbl, Offsets::hkbCharacterStringData_vtbl.address());
			return nullptr;
		}

		return stringData;
	}

	const char* SafeStr(const char* p)
	{
		if (!p) return "(null)";
		if (reinterpret_cast<uintptr_t>(p) < 0x10000) return "(invalid-ptr)";
		if (IsBadReadPtr(p, 1)) return "(unreadable)";
		return p;
	}

	void ProcessStringData(const char* a_hookName, RE::hkbCharacterStringData* a_stringData,
		const char* a_animationPath, bool& a_injected)
	{
		const char* safePath = SafeStr(a_animationPath);
		logger::info("[OAR] {}: valid stringData at {:X}, animPath='{}'",
			a_hookName, reinterpret_cast<uintptr_t>(a_stringData), safePath);

		auto* oar = OpenAnimationReplacer::GetSingleton();
		if (oar->GetTotalReplacementCount() > 0) {
			try {
				a_injected = oar->CreateReplacementAnimations(safePath, a_stringData);
			} catch (const std::exception& e) {
				logger::error("[OAR] Exception in CreateReplacementAnimations: {}", e.what());
			} catch (...) {
				logger::error("[OAR] Unknown exception in CreateReplacementAnimations");
			}
		} else {
			std::lock_guard lock(s_capturedMutex);
			s_capturedStringData = a_stringData;
			s_capturedAnimPath = (safePath && safePath[0] != '(') ? safePath : "";
			logger::info("[OAR] {}: no replacements yet, capturing stringData for deferred injection",
				a_hookName);
		}
	}

	void HandleLoadClipsCommon(const char* a_hookName,
		void* a_firstArg, const char* a_animationPath,
		bool& a_injected)
	{
		if (!a_firstArg || IsBadReadPtr(a_firstArg, sizeof(uintptr_t))) return;

		uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(a_firstArg);

		if (vtbl == Offsets::hkbCharacterStringData_vtbl.address()) {
			auto* stringData = reinterpret_cast<RE::hkbCharacterStringData*>(a_firstArg);
			ProcessStringData(a_hookName, stringData, a_animationPath, a_injected);
			return;
		}

		static uintptr_t bsGraphVtbl = REL::Relocation<uintptr_t>{ REL::ID(742655) }.address();
		static uintptr_t bindingSetVtbl = REL::Relocation<uintptr_t>{ REL::ID(802975) }.address();

		if (vtbl == bsGraphVtbl) {
			logger::info("[OAR] {}: received BShkbAnimationGraph at {:X}, extracting stringData...",
				a_hookName, reinterpret_cast<uintptr_t>(a_firstArg));

			auto* stringData = ExtractStringDataFromGraph(a_firstArg);
			if (stringData) {
				ProcessStringData(a_hookName, stringData, a_animationPath, a_injected);
			} else {
				std::lock_guard lock(s_capturedGraphsMutex);
				s_capturedGraphs.push_back(a_firstArg);
				logger::info("[OAR] {}: stringData not ready, captured graph for deferred extraction ({} total)",
					a_hookName, s_capturedGraphs.size());
			}
			return;
		}

		if (vtbl == bindingSetVtbl) {
			logger::info("[OAR] {}: received hkbAnimationBindingSet at {:X} (not extractable yet)",
				a_hookName, reinterpret_cast<uintptr_t>(a_firstArg));
			return;
		}

		if (Settings::GetSingleton()->bVerboseLogging) {
			logger::warn("[OAR] {}: unknown first arg vtable {:X}",
				a_hookName, vtbl);
		}
	}

	void LogAllArgs(const char* hookName, void* a1, void* a2, void* a3, void* a4, const char* a5, void* a6)
	{
		auto safeVtbl = [](void* p) -> uintptr_t {
			if (!p || IsBadReadPtr(p, sizeof(uintptr_t))) return 0;
			return *reinterpret_cast<uintptr_t*>(p);
		};

		logger::info("[OAR] {} args:", hookName);
		logger::info("[OAR]   arg1={:X} vtbl={:X}", (uintptr_t)a1, safeVtbl(a1));
		logger::info("[OAR]   arg2={:X} vtbl={:X}", (uintptr_t)a2, safeVtbl(a2));
		logger::info("[OAR]   arg3={:X} vtbl={:X}", (uintptr_t)a3, safeVtbl(a3));
		logger::info("[OAR]   arg4={:X} vtbl={:X}", (uintptr_t)a4, safeVtbl(a4));
		if (a5 && !IsBadReadPtr(a5, 1))
			logger::info("[OAR]   arg5(path)='{}'", a5);
		else
			logger::info("[OAR]   arg5={:X}", (uintptr_t)a5);
		logger::info("[OAR]   arg6={:X}", (uintptr_t)a6);
	}

	RE::hkbCharacterStringData* ScanForStringData(void* obj, size_t scanBytes)
	{
		if (!obj || IsBadReadPtr(obj, scanBytes)) return nullptr;
		uintptr_t sdVtbl = Offsets::hkbCharacterStringData_vtbl.address();
		auto* bytes = reinterpret_cast<uintptr_t*>(obj);
		size_t count = scanBytes / sizeof(uintptr_t);

		for (size_t i = 0; i < count; i++) {
			if (IsBadReadPtr(&bytes[i], sizeof(uintptr_t))) break;
			uintptr_t val = bytes[i];
			if (!val || val < 0x10000) continue;
			if (IsBadReadPtr(reinterpret_cast<void*>(val), sizeof(uintptr_t))) continue;
			uintptr_t candidateVtbl = *reinterpret_cast<uintptr_t*>(val);
			if (candidateVtbl == sdVtbl) {
				logger::info("[OAR]   ScanForStringData: found at offset +0x{:X} (ptr={:X})",
					i * sizeof(uintptr_t), val);
				return reinterpret_cast<RE::hkbCharacterStringData*>(val);
			}
		}
		return nullptr;
	}

	// Capture LoadClips path into the map AND log it.
	// Hook #1 provides valid path args (weapon-specific folders) on initial graph load.
	static void CaptureAndLogLoadClipsPath(const char* a_hookName, RE::hkbCharacterStringData* a_stringData, const char* a_animationPath)
	{
		if (!a_stringData || !a_animationPath) return;
		if (reinterpret_cast<uintptr_t>(a_stringData) < 0x10000) return;
		if (reinterpret_cast<uintptr_t>(a_animationPath) < 0x10000) return;
		if (IsBadReadPtr(a_animationPath, 1)) return;
		if (a_animationPath[0] == '\0') return;

		uintptr_t sdVtbl = Offsets::hkbCharacterStringData_vtbl.address();
		if (IsBadReadPtr(a_stringData, sizeof(uintptr_t))) return;
		uintptr_t actualVtbl = *reinterpret_cast<uintptr_t*>(a_stringData);
		if (actualVtbl != sdVtbl) return;

		std::string pathStr(a_animationPath);
		{
			std::unique_lock lock(s_loadClipsPathMutex);
			s_loadClipsPathMap[a_stringData] = pathStr;
		}

		static int s_loadClipsLogCount = 0;
		if (s_loadClipsLogCount < 30) {
			logger::info("[OAR-LoadClips] {}: stringData={:X} animPath='{}'",
				a_hookName, reinterpret_cast<uintptr_t>(a_stringData), pathStr);
			s_loadClipsLogCount++;
		}
	}

	void HookedLoadClips(RE::hkbCharacterStringData* a_stringData, void* a_bindingSet,
		void* a_assetLoader, RE::hkbBehaviorGraph* a_rootBehavior,
		const char* a_animationPath, void* a_annotationMap)
	{
		static bool s_logged = false;
		if (!s_logged) {
			LogAllArgs("HookedLoadClips[1]", a_stringData, a_bindingSet, a_assetLoader,
				a_rootBehavior, a_animationPath, a_annotationMap);
			s_logged = true;
		}

		CaptureAndLogLoadClipsPath("LoadClips1", a_stringData, a_animationPath);

		Hooks::LoadClipsHooks::_LoadClips(a_stringData, a_bindingSet, a_assetLoader,
			a_rootBehavior, a_animationPath, a_annotationMap);
	}

	void HookedLoadClips2(RE::hkbCharacterStringData* a_stringData, void* a_bindingSet,
		void* a_assetLoader, RE::hkbBehaviorGraph* a_rootBehavior,
		const char* a_animationPath, void* a_annotationMap)
	{
		static bool s_logged = false;
		if (!s_logged) {
			LogAllArgs("HookedLoadClips[2]", a_stringData, a_bindingSet, a_assetLoader,
				a_rootBehavior, a_animationPath, a_annotationMap);
			s_logged = true;
		}

		CaptureAndLogLoadClipsPath("LoadClips2", a_stringData, a_animationPath);

		// Dump the animationNames from this stringData for diagnosis
		static bool s_dumpedNames = false;
		if (!s_dumpedNames && a_stringData && reinterpret_cast<uintptr_t>(a_stringData) > 0x10000 &&
			!IsBadReadPtr(a_stringData, sizeof(void*)))
		{
			uintptr_t sdVtbl = Offsets::hkbCharacterStringData_vtbl.address();
			uintptr_t actualVtbl = *reinterpret_cast<uintptr_t*>(a_stringData);
			if (actualVtbl == sdVtbl) {
				auto& animNames = a_stringData->animationNames;
				auto* arrBase = reinterpret_cast<const uint8_t*>(&animNames);
				auto* nameData = *reinterpret_cast<RE::hkbCharacterStringData::FileNameMeshNamePair* const*>(arrBase);
				int32_t nameSize = *reinterpret_cast<const int32_t*>(arrBase + 8);

				logger::info("[OAR-LoadClips2-Dump] stringData={:X} animNames.size={}", 
					reinterpret_cast<uintptr_t>(a_stringData), nameSize);

				if (nameData && !IsBadReadPtr(nameData, sizeof(void*)) && nameSize > 0) {
					int dumpCount = std::min(nameSize, 20);
					for (int i = 0; i < dumpCount; i++) {
						const char* fn = nameData[i].fileName.data();
						if (fn && reinterpret_cast<uintptr_t>(fn) > 0x10000 && !IsBadReadPtr(fn, 1)) {
							logger::info("[OAR-LoadClips2-Dump]   [{}] fileName='{}'", i, fn);
						} else {
							logger::info("[OAR-LoadClips2-Dump]   [{}] fileName=(bad ptr {:X})", i, reinterpret_cast<uintptr_t>(fn));
						}
					}
				}
				s_dumpedNames = true;
			}
		}

		Hooks::LoadClipsHooks::_LoadClips2(a_stringData, a_bindingSet, a_assetLoader,
			a_rootBehavior, a_animationPath, a_annotationMap);
	}
}

OARClipPerspective GetPlayingClipPerspective(RE::hkbClipGenerator* a_clip)
{
	return GetPlayingClipPerspectiveImpl(a_clip);
}

namespace Hooks
{
	// === NotifyAnimationGraphImpl hook via Actor + PlayerCharacter vtables ===
	// IAnimationGraphManagerHolder::NotifyAnimationGraphImpl is vfunc index 1
	// on the IAnimationGraphManagerHolder vtable (Actor / PlayerCharacter vtable
	// array index 5). Dual purpose:
	//   1) ActionFireEmpty detection for IsDryFiringCondition
	//   2) Thread-local nesting depth so replacement annotation firing can refuse
	//      to nest a second Notify inside an outer one (collectActiveNodes CTD).
	namespace ActionFireEmptyHook
	{
		using NotifyFn = bool(*)(RE::IAnimationGraphManagerHolder*, const RE::BSFixedString&);
		static NotifyFn _OriginalNotify = nullptr;

		// SEH wrapper must live in its own function — MSVC forbids __try in the
		// same function as C++ objects with destructors (unique_lock below).
		static bool CallOriginalNotifySEH(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName)
		{
			__try {
				return _OriginalNotify(a_this, a_eventName);
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				return false;
			}
		}

		static bool HookedNotifyAnimGraph(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName)
		{
			// Check if this is ActionFireEmpty
			const char* evtStr = a_eventName.c_str();
			if (evtStr && _stricmp(evtStr, "ActionFireEmpty") == 0) {
				// Resolve the actor from the IAnimationGraphManagerHolder pointer.
				// IAnimationGraphManagerHolder is at offset 0x048 in TESObjectREFR,
				// so subtract 0x048 to get back to the TESObjectREFR base.
				auto* refr = reinterpret_cast<RE::TESObjectREFR*>(
					reinterpret_cast<uintptr_t>(a_this) - 0x48);
				if (refr) {
					uint32_t formID = refr->GetFormID();
					std::unique_lock lock{ s_fireEmptyMutex };
					auto& entry = s_fireEmptyMap[formID];
					entry.timestamp = std::chrono::steady_clock::now();
					entry.generation++;
				}
			}

			// Depth wraps the original call so any clip Update/Activate that runs
			// as a consequence of this notify sees s_notifyAnimGraphDepth > 0.
			++s_notifyAnimGraphDepth;
			const bool result = CallOriginalNotifySEH(a_this, a_eventName);
			--s_notifyAnimGraphDepth;
			return result;
		}

		void Install()
		{
			// Actor IAnimationGraphManagerHolder vtable = Actor VTABLE array index 5
			REL::Relocation<uintptr_t> actorAnimGraphVtbl{ REL::ID(453840) };
			_OriginalNotify = reinterpret_cast<NotifyFn>(
				actorAnimGraphVtbl.write_vfunc(1, &HookedNotifyAnimGraph));

			// PlayerCharacter has its own copy of that interface vtable (array index 5).
			// Hook it too so player-side notifies (the crash path) update the depth.
			REL::Relocation<uintptr_t> playerAnimGraphVtbl{ REL::ID(1276847) };
			auto* playerPrev = reinterpret_cast<NotifyFn>(
				playerAnimGraphVtbl.write_vfunc(1, &HookedNotifyAnimGraph));
			if (playerPrev != _OriginalNotify && playerPrev != &HookedNotifyAnimGraph) {
				logger::warn("[OAR] PlayerCharacter NotifyAnimationGraphImpl differs from Actor "
					"({:X} vs {:X}) — depth tracking uses Actor original",
					reinterpret_cast<uintptr_t>(playerPrev),
					reinterpret_cast<uintptr_t>(_OriginalNotify));
			}

			logger::info("[OAR] NotifyAnimationGraphImpl hooks installed (Actor + PlayerCharacter; ActionFireEmpty + reentrancy depth)");
		}
	}

	// === SetupSpecialIdle interception for filter-only special-idle layers ===
	namespace SetupSpecialIdleHook
	{
		using SetupFn = bool(*)(RE::AIProcess*, RE::Actor&, RE::DEFAULT_OBJECT,
			RE::TESIdleForm*, bool, RE::TESObjectREFR*);
		static SetupFn _Original = nullptr;

		static bool HookedSetupSpecialIdle(RE::AIProcess* a_process, RE::Actor& a_actor,
			RE::DEFAULT_OBJECT a_defaultObject, RE::TESIdleForm* a_idle,
			bool a_testConditions, RE::TESObjectREFR* a_targetOverride)
		{
			const auto actorFormID = a_actor.GetFormID();
			const auto idleFormID = a_idle ? a_idle->GetFormID() : 0;
			const char* fileName = a_idle ? a_idle->animFileName.c_str() : nullptr;
			logger::info(
				"[OAR-TrackFilter-Standalone] SetupSpecialIdle entry: actor={:08X}, idle={:08X}, file='{}', defaultObject={}, testConditions={}",
				actorFormID, idleFormID, fileName ? fileName : "<null>",
				static_cast<std::uint32_t>(a_defaultObject), a_testConditions);

			// The intercepted path must honor the IDLE record's own conditions;
			// otherwise we could report success for an idle vanilla would reject.
			const bool idleConditionsPass = a_idle && (!a_testConditions ||
				a_idle->CheckConditions(&a_actor, a_targetOverride, true));
			std::string rejectionReason;
			if (idleConditionsPass && StartStandaloneSpecialIdle(&a_actor, a_idle, &rejectionReason)) {
				// Report success to the PlayIdle caller without creating the native
				// special-idle state or its behavior-authored end trigger.
				return true;
			}
			if (!idleConditionsPass) {
				rejectionReason = "TESIdleForm conditions evaluated false";
			}
			logger::info(
				"[OAR-TrackFilter-Standalone] SetupSpecialIdle fallback: actor={:08X}, idle={:08X}, reason='{}'",
				actorFormID, idleFormID,
				rejectionReason.empty() ? "no filter-only match" : rejectionReason);
			return _Original ? _Original(a_process, a_actor, a_defaultObject, a_idle,
				a_testConditions, a_targetOverride) : false;
		}

		void Install()
		{
			REL::Relocation<std::uintptr_t> setupTarget{ RE::ID::AIProcess::SetupSpecialIdle };
			const auto target = setupTarget.address();
			if (!target) {
				logger::warn("[OAR-TrackFilter-Standalone] SetupSpecialIdle entry hook unavailable: runtime address is null");
				return;
			}

			MEMORY_BASIC_INFORMATION memoryInfo{};
			if (::VirtualQuery(reinterpret_cast<const void*>(target), &memoryInfo, sizeof(memoryInfo)) == 0 ||
				memoryInfo.State != MEM_COMMIT) {
				logger::warn("[OAR-TrackFilter-Standalone] SetupSpecialIdle entry hook unavailable: target memory is not committed");
				return;
			}
			const DWORD protection = memoryInfo.Protect & 0xFF;
			if (protection != PAGE_EXECUTE && protection != PAGE_EXECUTE_READ &&
				protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY) {
				logger::warn("[OAR-TrackFilter-Standalone] SetupSpecialIdle entry hook unavailable: target memory is not executable");
				return;
			}

			const auto initStatus = MH_Initialize();
			if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
				logger::warn("[OAR-TrackFilter-Standalone] MinHook initialization failed: {}",
					MH_StatusToString(initStatus));
				return;
			}

			const auto createStatus = MH_CreateHook(
				reinterpret_cast<void*>(target),
				reinterpret_cast<void*>(&HookedSetupSpecialIdle),
				reinterpret_cast<void**>(&_Original));
			if (createStatus != MH_OK) {
				logger::warn("[OAR-TrackFilter-Standalone] SetupSpecialIdle entry hook creation failed: {}",
					MH_StatusToString(createStatus));
				_Original = nullptr;
				return;
			}
			if (!_Original) {
				logger::warn("[OAR-TrackFilter-Standalone] SetupSpecialIdle entry hook produced no original trampoline; hook removed");
				MH_RemoveHook(reinterpret_cast<void*>(target));
				return;
			}
			const auto enableStatus = MH_EnableHook(reinterpret_cast<void*>(target));
			if (enableStatus != MH_OK) {
				logger::warn("[OAR-TrackFilter-Standalone] SetupSpecialIdle entry hook enable failed: {}",
					MH_StatusToString(enableStatus));
				MH_RemoveHook(reinterpret_cast<void*>(target));
				_Original = nullptr;
				return;
			}
			logger::info(
				"[OAR-TrackFilter-Standalone] Hooked SetupSpecialIdle function entry at {:X}; all direct and indirect PlayIdle callers are covered",
				target);
		}
	}

	// === Animation event feed via BSTEventSink<BSAnimationGraphEvent> vfunc hook ===
	// NG/AE replacement for the OG-only registered-sink path (RegisterSuppressionSink
	// uses BGSAnimationSystemUtils::GetEventSourcePointersFromGraph, id 897074, which
	// has no NG/AE Address Library entry). TESObjectREFR itself derives from
	// BSTEventSink<BSAnimationGraphEvent> (base at +0x38), and the engine delivers
	// every graph event to that sink — so hooking ProcessEvent (vfunc index 1) on
	// the Actor / PlayerCharacter copies of that base vtable gives us the same feed
	// with only RTTI-derived vtable IDs, which are identical across OG/NG/AE:
	//   Actor vtable array index 3           = REL::ID(720550)
	//   PlayerCharacter vtable array index 3 = REL::ID(1542933)
	// (index 5 is the IAnimationGraphManagerHolder base hooked above; the graph-event
	// sink sits two bases earlier: 030 BSActiveGraphIfInactiveEvent sink, 038 THIS,
	// 040 BGSInventoryListEvent sink, 048 holder — verified in the fork's
	// TESObjectREFR.h and against the 163/984/221 databases.)
	// Installed only on NG/AE; OG keeps the proven registered-sink path.
	namespace AnimGraphEventFeedHook
	{
		using ProcessEventFn = RE::BSEventNotifyControl(*)(
			void*, const RE::BSAnimationGraphEvent&, RE::BSTEventSource<RE::BSAnimationGraphEvent>*);
		static ProcessEventFn _OriginalActorProcess = nullptr;
		static ProcessEventFn _OriginalPlayerProcess = nullptr;

		static RE::BSEventNotifyControl HookedProcessEventImpl(
			void* a_sinkThis, const RE::BSAnimationGraphEvent& a_event,
			RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source,
			ProcessEventFn a_original)
		{
			const char* evtStr = a_event.tag.c_str();
			// The sink base lives at TESObjectREFR + 0x38 (fork header), so
			// back-adjust the interface pointer to the refr base. This is needed
			// even when event logging is disabled because IdleStop suppression is
			// an actor-scoped gameplay action.
			auto* refr = reinterpret_cast<RE::TESObjectREFR*>(
				reinterpret_cast<uintptr_t>(a_sinkThis) - 0x38);
			const bool applyIdleStopFix = evtStr && _stricmp(evtStr, "IdleStop") == 0 &&
				ConsumeIdleStopSuppression(refr);

			// OG keeps its proven registered-sink event log. This vfunc hook is
			// installed there only so it can bypass the actor's IdleStop handler.
			if (!REX::FModule::IsRuntimeOG() && evtStr && evtStr[0] &&
				AnimationLog::GetSingleton()->IsEnabled()) {
				// Display tag.payload for dotted annotation events (see the
				// registered-sink feed for rationale).
				std::string display(evtStr);
				if (const char* pay = a_event.payload.c_str(); pay && pay[0]) {
					display += '.';
					display += pay;
				}
				AnimationLog::GetSingleton()->AddAnimEvent(refr, display);
			}
			if (applyIdleStopFix) {
				// Match fallout4-idlestopfix: force the animation manager past the
				// lingering idle, then still deliver IdleStop to the original actor
				// sink. Consuming the event here left other graph state unfinished.
				static thread_local bool applyingFix = false;
				if (!applyingFix) {
					if (auto* actor = refr ? refr->As<RE::Actor>() : nullptr) {
						applyingFix = true;
						actor->UpdateAnimation(1000.0f);
						applyingFix = false;
						logger::info(
							"[OAR-IdleStop] Fast-forwarded actor {:X} by 1000s before delivering IdleStop",
							actor->GetFormID());
					}
				}
			}
			return a_original ? a_original(a_sinkThis, a_event, a_source) :
				RE::BSEventNotifyControl::kContinue;
		}

		static RE::BSEventNotifyControl HookedActorProcessEvent(
			void* a_this, const RE::BSAnimationGraphEvent& a_event,
			RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source)
		{
			return HookedProcessEventImpl(a_this, a_event, a_source, _OriginalActorProcess);
		}

		static RE::BSEventNotifyControl HookedPlayerProcessEvent(
			void* a_this, const RE::BSAnimationGraphEvent& a_event,
			RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source)
		{
			return HookedProcessEventImpl(a_this, a_event, a_source, _OriginalPlayerProcess);
		}

		void Install()
		{
			REL::Relocation<uintptr_t> actorSinkVtbl{ REL::ID(720550) };
			_OriginalActorProcess = reinterpret_cast<ProcessEventFn>(
				actorSinkVtbl.write_vfunc(1, &HookedActorProcessEvent));

			REL::Relocation<uintptr_t> playerSinkVtbl{ REL::ID(1542933) };
			_OriginalPlayerProcess = reinterpret_cast<ProcessEventFn>(
				playerSinkVtbl.write_vfunc(1, &HookedPlayerProcessEvent));

			logger::info("[OAR] Animation event sink hooks installed (Actor + PlayerCharacter; IdleStop suppression{})",
				REX::FModule::IsRuntimeOG() ? "; OG logging remains on registered sink" : "; event logging enabled");
		}
	}

	// Secondary detection: hook the PlayerControls "fire empty" code path directly.
	// REL::ID 818081 (OG) / 2234796 (NG; AE bins carry the same id) is
	// PlayerControls::DoAction itself. When the player presses fire with an empty
	// weapon, the attack-processing path inside DoAction recursively calls itself
	// to start the auto-reload. AE 1.11.221 codegen duplicates that recursion
	// across two branches (disassembly of the on-disk exe at the AL RVA):
	//   +0x464: DoAction(this, kActionReload = 0x6C, 2)  -- empty mag, ammo type equipped
	//   +0x477: DoAction(this, action = 0x75, 2)         -- no ammo type equipped at all
	// while OG shares a single call site at +0x40A. Both AE branches indicate
	// "player pressed fire with an empty weapon", so both are hooked for
	// detection. Hooking here is upstream of the animation graph, so it fires
	// even if the anim event is swallowed before NotifyAnimationGraphImpl.
	//
	// Because the displacement moves between runtimes, the sites are located by
	// scanning the function body for CALL (E8) instructions whose argument setup
	// stages the 0x6C / 0x75 action constant. The CALL target is accepted if it
	// is the function start (pristine self-call) OR lies outside the game module
	// (another mod, e.g. ManualReloadF4SE, already detoured the same site into
	// its trampoline — GLXRM_ManualReload.dll loads before us alphabetically and
	// swallows the auto-reload; chaining through it is intended). The OG-measured
	// +0x40A site remains as a guarded fallback (the OG Steam exe is DRM-wrapped
	// on disk, so that site can only be runtime-verified — and it is, on 1.10.163).
	namespace PlayerFireEmptyHook
	{
		using DoActionFn = int64_t(*)(int64_t, int, unsigned int);

		// Per-site trampoline originals; AE has two distinct call sites and each
		// chains to its own original (which may be another mod's detour).
		static DoActionFn _OriginalDoAction[2] = { nullptr, nullptr };

		static void RecordFireEmpty()
		{
			// The player just pressed fire with an empty weapon — record timestamp.
			// We use formID 0x14 (player) since this only fires for the player character.
			std::unique_lock lock{ s_fireEmptyMutex };
			auto& entry = s_fireEmptyMap[0x14];
			entry.timestamp = std::chrono::steady_clock::now();
			entry.generation++;
		}

		// One thin detour per call site. Behavior: record the fire-empty press
		// (feeds IsFiringEmpty conditions and the auto-reload replication),
		// then swallow the engine's auto-reload — it is attack-initiated and
		// gets cut short at reloadComplete. OAR's replacement reload (see
		// AutoReloadSuppression::PerFrameUpdate) goes through the reload-key
		// path instead and plays in full. The originals are kept only so the
		// call chain stays intact if a future mode needs to call through.
		// Dry-fire click. With the engine's fire-empty auto-reload suppressed,
		// pressing fire on an empty magazine would otherwise give no feedback at
		// all: vanilla plays no sound of its own on this path (it auto-reloads
		// instead), which is exactly why the original Manual Reload mod added
		// one in its v1.2. Plays the equipped weapon's Attack Fail sound
		// descriptor, falling back to the vanilla WPNPistol10mmFireDry click
		// when the weapon has none (same default the original mod uses).
		// Runs on the game thread (inside PlayerControls::DoAction).
		static void PlayDryFireClick()
		{
			if (!Settings::GetSingleton()->bPlayDryFireSound) {
				return;
			}

			// Debounce: automatic weapons re-attempt the fire action every frame
			// while the trigger is held, which would buzz the click at frame
			// rate. 150 ms still lets rapid deliberate presses each click.
			static std::chrono::steady_clock::time_point s_lastClick{};
			const auto now = std::chrono::steady_clock::now();
			if (now - s_lastClick < std::chrono::milliseconds(150)) {
				return;
			}
			s_lastClick = now;

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->currentProcess || !player->currentProcess->middleHigh) {
				return;
			}

			// Find the equipped weapon's attack-fail sound. The equipped item's
			// live instance data (weapon mods applied) wins over the base form's
			// default data.
			RE::BGSSoundDescriptorForm* failSound = nullptr;
			{
				auto* mh = player->currentProcess->middleHigh;
				RE::BSAutoLock lock{ mh->equippedItemsLock };
				for (auto& eq : mh->equippedItems) {
					auto* weap = eq.item.object ? eq.item.object->As<RE::TESObjectWEAP>() : nullptr;
					if (!weap) {
						continue;
					}
					if (auto* inst = static_cast<RE::TESObjectWEAP::InstanceData*>(eq.item.instanceData.get())) {
						failSound = inst->attackFailSound;
					}
					if (!failSound) {
						failSound = weap->weaponData.attackFailSound;
					}
					break;
				}
			}

			// BSAudioManager resolves sounds by editor id (SNDR editor ids are
			// kept at runtime for exactly this lookup). If the weapon-specific
			// descriptor is missing or fails to resolve, use the vanilla click.
			const char* name = failSound ? failSound->GetFormEditorID() : nullptr;
			if (!name || !name[0] || !PlaySoundDirect(name, player)) {
				PlaySoundDirect("WPNPistol10mmFireDry", player);
			}
		}

		static int64_t HookedDoAction0(int64_t a_arg1, int a_arg2, unsigned int a_arg3)
		{
			(void)a_arg1; (void)a_arg2; (void)a_arg3;
			RecordFireEmpty();
			PlayDryFireClick();
			return 0;
		}

		static int64_t HookedDoAction1(int64_t a_arg1, int a_arg2, unsigned int a_arg3)
		{
			(void)a_arg1; (void)a_arg2; (void)a_arg3;
			RecordFireEmpty();
			PlayDryFireClick();
			return 0;
		}

		// Returns [start, end) of the game module for the "target outside module"
		// test (a rel32 rewritten by another mod points into its trampoline, which
		// lives outside the exe image).
		static std::pair<std::uintptr_t, std::uintptr_t> GetGameModuleRange()
		{
			const auto modBase = reinterpret_cast<std::uintptr_t>(::GetModuleHandleW(nullptr));
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(modBase);
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(modBase + dos->e_lfanew);
			return { modBase, modBase + nt->OptionalHeader.SizeOfImage };
		}

		// Scans the first a_scanLen bytes of the function at a_base for the
		// recursive auto-reload CALL site(s), identified by the staged action
		// constant (0x6C kActionReload / 0x75 no-ammo variant) in the ~16 bytes
		// before the CALL. Fills a_sites (up to a_maxSites) and returns the count.
		static std::size_t FindFireEmptyCallSites(std::uintptr_t a_base, std::size_t a_scanLen, std::uintptr_t* a_sites, std::size_t a_maxSites)
		{
			const auto [modStart, modEnd] = GetGameModuleRange();
			const auto* code = reinterpret_cast<const std::uint8_t*>(a_base);
			std::size_t count = 0;

			for (std::size_t i = 0; i + 5 <= a_scanLen && count < a_maxSites; ++i) {
				if (code[i] != 0xE8)
					continue;
				const auto rel = *reinterpret_cast<const std::int32_t*>(code + i + 1);
				const auto target = a_base + i + 5 + static_cast<std::intptr_t>(rel);

				// Accept a pristine self-call or a site already detoured out of
				// the module by another mod; reject calls to other game functions.
				const bool selfCall = (target == a_base);
				const bool detoured = (target < modStart || target >= modEnd);
				if (!selfCall && !detoured)
					continue;

				// Confirm the action-id constant is staged just before the call:
				//   lea edx, [rax+6Ch]  -> 8D 50 6C      (AE +0x464 codegen)
				//   mov edx, 75h        -> BA 75 00 00 00 (AE +0x477 codegen)
				//   plus the mov/lea variants of each, in case codegen differs.
				const std::size_t lookback = i >= 16 ? i - 16 : 0;
				for (std::size_t j = lookback; j + 3 <= i; ++j) {
					const bool leaReload = (code[j] == 0x8D && code[j + 1] == 0x50 && (code[j + 2] == 0x6C || code[j + 2] == 0x75));
					const bool movReload = (code[j] == 0xBA && (code[j + 1] == 0x6C || code[j + 1] == 0x75) && code[j + 2] == 0x00);
					if (leaReload || movReload) {
						a_sites[count++] = a_base + i;
						break;
					}
				}
			}
			return count;
		}

		void Install(REL::Trampoline& trampoline)
		{
			REL::Relocation<std::uintptr_t> funcBase{ REL::ID({ 818081, 2234796 }) };
			const auto base = funcBase.address();

			// Primary: locate the recursive DoAction call site(s) by signature.
			std::uintptr_t sites[2] = { 0, 0 };
			std::size_t count = FindFireEmptyCallSites(base, 0x800, sites, 2);

			// Fallback: the OG-measured +0x40A displacement, guarded by opcode check.
			if (count == 0) {
				const auto* legacy = reinterpret_cast<const std::uint8_t*>(base + 0x40A);
				if (*legacy == 0xE8) {
					sites[count++] = base + 0x40A;
				}
			}

			if (count == 0) {
				logger::warn("[OAR] PlayerFireEmpty hook skipped: no auto-reload call site found in 818081/2234796 and no CALL at legacy +0x40A; fire-empty detection falls back to the anim-graph path");
				return;
			}

			static constexpr DoActionFn kDetours[2] = { &HookedDoAction0, &HookedDoAction1 };
			for (std::size_t i = 0; i < count; ++i) {
				_OriginalDoAction[i] = reinterpret_cast<DoActionFn>(
					trampoline.write_call<5>(sites[i], kDetours[i]));
				logger::info("[OAR] PlayerFireEmpty hook installed (REL::ID 818081/2234796 + 0x{:X})", sites[i] - base);
			}
		}
	}

	// Last-round auto-reload suppression (the other half; the fire-on-empty
	// half lives in PlayerFireEmptyHook's detours above). Always applied: the
	// engine's auto-reloads are attack-initiated and get cut short, so OAR
	// replaces them entirely (see the Auto-Reload mode / PerFrameUpdate below).
	//
	// PlayerCharacter::UseAmmo (REL::ID 902833 OG / 2233939 NG+AE, vtable slot
	// 0xF0) auto-reloads when the last round is consumed. At +0x206 a near-JZ
	// (0F 84) guards the auto-reload; rewriting it to NOP+JMP (90 E9) forces the
	// "skip auto-reload" branch while keeping the original rel32 displacement.
	// Same patch as ManualReloadF4SE. Coexistence: if that plugin already
	// applied it, the bytes read 90 E9 and we leave them alone.
	namespace AutoReloadSuppression
	{
		static constexpr std::uint8_t kOriginalBytes[2] = { 0x0F, 0x84 };  // jz near
		static constexpr std::uint8_t kPatchedBytes[2] = { 0x90, 0xE9 };   // nop; jmp near

		void Install()
		{
			// { OG, NG }; AE shares the NG id. The +0x206 displacement is
			// OG-measured but holds on AE 1.11.221 (the opcode guard below
			// verifies before writing).
			REL::Relocation<std::uintptr_t> site{ REL::ID({ 902833, 2233939 }), 0x206 };
			const auto address = site.address();

			const auto* bytes = reinterpret_cast<const std::uint8_t*>(address);
			if (bytes[0] == kPatchedBytes[0] && bytes[1] == kPatchedBytes[1]) {
				// Another plugin (ManualReloadF4SE) already suppresses this path.
				logger::info("[OAR] Last-round auto-reload already patched by another plugin; leaving as is");
				return;
			}
			if (bytes[0] != kOriginalBytes[0] || bytes[1] != kOriginalBytes[1]) {
				logger::warn("[OAR] Last-round auto-reload patch skipped: expected JZ near (0F 84) at 902833/2233939+0x206, found {:02X} {:02X}", bytes[0], bytes[1]);
				return;
			}
			if (!REL::WriteSafe(address, kPatchedBytes, sizeof(kPatchedBytes))) {
				logger::error("[OAR] Last-round auto-reload patch: WriteSafe failed at {:X}", address);
				return;
			}
			logger::info("[OAR] Last-round auto-reload patch applied (REL::ID 902833/2233939 + 0x206)");
		}

		// ===== Auto-reload replication ==========================================
		// Replaces the vanilla auto-reload convenience while the engine's own
		// (attack-initiated, truncating) auto-reloads stay suppressed. OAR calls
		// PlayerControls::DoAction(kActionReload, kTry) from the per-frame
		// update — the same action id and priority the reload key uses (all of
		// the game's own reload call sites stage kActionReload with kTry,
		// verified by scanning the AE exe's 52 DoAction callers).
		//
		// The trigger is picked by Settings > General > Auto-Reload (dropdown):
		//   0 = last round:  the equipped weapon's magazine count transitions
		//                    >0 -> 0 on the same weapon (i.e. by firing, not by
		//                    switching to an already-empty weapon)
		//   1 = fire empty:  the PlayerFireEmptyHook generation counter advanced
		//                    (player pressed fire with an empty magazine)
		//   2 = suppress:    no automatic reloads at all; reload key only
		//
		// CRITICAL TIMING: the reload must NOT be issued while the firing /
		// dry-firing attack action is still current (gunState kFire /
		// kFireSighted). A reload started inside that window is owned by the
		// attack context and the engine exits the reload state the moment
		// reloadComplete refills the magazine — the exact truncation this
		// feature exists to avoid (and DoAction can also outright reject the
		// reload during the attack, losing it entirely). So triggers only ARM
		// a pending reload here; it is ISSUED once the player's gun state
		// leaves the attack states, and retried until DoAction accepts it.
		// DoAction(kTry) validates the rest (reserve ammo, menus, weapon
		// state) exactly like a reload-key press would.

		static const void* s_prevWeaponData = nullptr;  // EquippedWeaponData identity
		static std::uint32_t s_prevMagCount = 0;
		static bool s_prevMagValid = false;
		static std::uint32_t s_lastFireEmptyGen = 0;
		static bool s_fireEmptyGenValid = false;

		static bool s_reloadPending = false;
		static std::uint32_t s_pendingFrames = 0;
		// Give up if the reload could not be issued within ~5s worth of frames
		// (weapon holstered mid-pending, scripted scenes, etc.).
		static constexpr std::uint32_t kPendingFrameLimit = 600;

		// Reads the player's equipped weapon's magazine count (same walk as
		// CurrentMagazineAmmoCondition). Returns false when no gun with an ammo
		// type is equipped.
		static bool GetPlayerMagCount(std::uint32_t& a_outCount, const void*& a_outWeaponData)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->currentProcess || !player->currentProcess->middleHigh) {
				return false;
			}
			auto* mh = player->currentProcess->middleHigh;
			RE::BSAutoLock lock{ mh->equippedItemsLock };
			for (auto& eq : mh->equippedItems) {
				if (!eq.data) {
					continue;
				}
				auto* wd = static_cast<RE::EquippedWeaponData*>(eq.data.get());
				if (!wd || !wd->ammo) {
					continue;
				}
				a_outCount = static_cast<std::uint32_t>(wd->ammoCount);
				a_outWeaponData = wd;
				return true;
			}
			return false;
		}

		static void CancelPending()
		{
			s_reloadPending = false;
			s_pendingFrames = 0;
		}

		static void ArmPending(const char* a_reason)
		{
			if (!s_reloadPending) {
				s_reloadPending = true;
				s_pendingFrames = 0;
				static int s_armLog = 0;
				if (s_armLog < 20) {
					logger::info("[OAR] Auto-reload armed ({})", a_reason);
					s_armLog++;
				}
			}
		}

		// Runs every frame on the game thread (called from HookedActorUpdate
		// once the game is fully loaded).
		void PerFrameUpdate()
		{
			const int mode = Settings::GetSingleton()->iAutoReloadMode;

			// Mode 2 (suppress only): drop state so nothing stale triggers later.
			if (mode == 2) {
				s_prevMagValid = false;
				s_fireEmptyGenValid = false;
				CancelPending();
				return;
			}

			std::uint32_t magCount = 0;
			const void* weaponData = nullptr;
			if (!GetPlayerMagCount(magCount, weaponData)) {
				s_prevMagValid = false;
				CancelPending();
				return;
			}

			// Mode 0 — last round: mag transitioned >0 -> 0 on the SAME weapon.
			// A weapon-data identity change means equip/switch, not firing.
			if (s_prevMagValid && weaponData == s_prevWeaponData) {
				if (mode == 0 && s_prevMagCount > 0 && magCount == 0) {
					ArmPending("last round");
				}
			} else {
				CancelPending();  // weapon changed: any pending reload is stale
			}
			s_prevWeaponData = weaponData;
			s_prevMagCount = magCount;
			s_prevMagValid = true;

			// Mode 1 — fire press when empty: the PlayerFireEmptyHook detour
			// advanced the generation counter (covers both the 0x6C and 0x75
			// swallowed engine branches).
			{
				const std::uint32_t gen = GetFireEmptyGeneration(0x14);
				if (!s_fireEmptyGenValid) {
					s_lastFireEmptyGen = gen;  // first frame: sync, don't trigger
					s_fireEmptyGenValid = true;
				} else if (gen != s_lastFireEmptyGen) {
					s_lastFireEmptyGen = gen;
					if (mode == 1 && magCount == 0) {
						ArmPending("fire press on empty");
					}
				}
			}

			if (!s_reloadPending) {
				return;
			}

			// Pending reload: decide whether to cancel, wait, or issue.
			if (magCount != 0) {
				CancelPending();  // mag refilled: a reload already happened
				return;
			}
			if (++s_pendingFrames > kPendingFrameLimit) {
				CancelPending();
				return;
			}

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}
			const auto gunState = player->gunState;
			if (gunState == RE::GUN_STATE::kReloading) {
				CancelPending();  // a reload (ours or manual) is already running
				return;
			}
			// Attack action still current (firing / dry-fire click / grenade):
			// wait for it to finish so the reload is not owned by the attack
			// context (truncation) or rejected outright.
			if (gunState == RE::GUN_STATE::kFire ||
				gunState == RE::GUN_STATE::kFireSighted ||
				gunState == RE::GUN_STATE::kThrowing) {
				return;
			}

			if (auto* controls = RE::PlayerControls::GetSingleton()) {
				const bool ok = controls->DoAction(
					RE::DEFAULT_OBJECT::kActionReload,
					RE::ActionInput::ACTIONPRIORITY::kTry);
				static int s_issueLog = 0;
				if (s_issueLog < 20) {
					logger::info("[OAR] Auto-reload issued: DoAction(kActionReload) -> {} (gunState={})",
						ok, static_cast<int>(gunState));
					s_issueLog++;
				}
				if (ok) {
					CancelPending();
				}
				// Rejected: stay pending and retry next frame until the limit.
			}
		}
	}

	namespace TrackFilterCameraHooks
	{
		static bool IsFirstPersonCamera()
		{
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (!camera || !camera->currentState) return false;
			const auto id = camera->currentState->id;
			return id == RE::CameraStates::kFirstPerson || id == RE::CameraStates::kIronSights;
		}

		static RE::NiAVObject* FindCameraNode(RE::PlayerCharacter* a_player, RE::NiAVObject*& a_root)
		{
			a_root = a_player ? a_player->Get3D(true) : nullptr;
			if (!a_root) return nullptr;
			return a_root->GetObjectByName(RE::BSFixedString("Camera"));
		}

		static void RestorePreviousContribution(RE::PlayerCharacter* a_player)
		{
			if (!s_appliedTrackFilterCamera.valid) return;

			RE::NiAVObject* root = nullptr;
			auto* node = FindCameraNode(a_player, root);
			if (root == s_appliedTrackFilterCamera.root &&
				node == s_appliedTrackFilterCamera.node &&
				CameraTransformNear(node->local, s_appliedTrackFilterCamera.applied)) {
				node->local = s_appliedTrackFilterCamera.base;
			} else {
				// Another camera plugin or a skeleton rebuild changed the node after
				// OAR. Do not overwrite that newer state. The engine's animation update
				// that follows normally regenerates the Camera local from the graph.
				static std::atomic<uint32_t> s_restoreSkippedLogCount{ 0 };
				if (s_restoreSkippedLogCount.fetch_add(1, std::memory_order_relaxed) < 8) {
					logger::info(
						"[OAR-TrackFilter-Camera] Previous scene-node contribution was already "
						"changed; skipping restoration before animation evaluation");
				}
			}
			s_appliedTrackFilterCamera = {};
		}

		static void ApplyCurrentContribution(RE::PlayerCharacter* a_player, uint64_t a_evaluation)
		{
			if (!a_player || !IsFirstPersonCamera()) return;

			struct CameraDelta
			{
				float translation[3];
				float rotation[4];
			};
			std::vector<CameraDelta> deltas;
			{
				std::shared_lock lock(s_trackFilterMutex);
				auto statesIt = s_charTrackFilterMap.find(a_player);
				if (statesIt == s_charTrackFilterMap.end()) return;
				for (const auto& state : statesIt->second) {
					if (!state.pendingCameraValid ||
						state.pendingCameraEvaluation != a_evaluation) {
						continue;
					}
					CameraDelta delta{};
					std::copy_n(state.pendingCameraTranslationDelta, 3, delta.translation);
					std::copy_n(state.pendingCameraRotationDelta, 4, delta.rotation);
					deltas.push_back(delta);
				}
			}
			if (deltas.empty()) return;

			RE::NiAVObject* root = nullptr;
			auto* node = FindCameraNode(a_player, root);
			if (!node || !root) {
				static std::atomic<bool> s_missingCameraWarned{ false };
				if (!s_missingCameraWarned.exchange(true)) {
					logger::warn(
						"[OAR-TrackFilter-Camera] First-person Camera scene node was not found; "
						"sampled Camera motion was not applied");
				}
				return;
			}

			s_appliedTrackFilterCamera.root = root;
			s_appliedTrackFilterCamera.node = node;
			s_appliedTrackFilterCamera.base = node->local;
			for (const auto& delta : deltas) {
				node->local.translate.x += delta.translation[0];
				node->local.translate.y += delta.translation[1];
				node->local.translate.z += delta.translation[2];
				node->local.rotate = CameraQuatToMatrix(delta.rotation) * node->local.rotate;
			}
			s_appliedTrackFilterCamera.applied = node->local;
			s_appliedTrackFilterCamera.valid = true;

			static std::atomic<uint32_t> s_sceneApplyLogCount{ 0 };
			if (s_sceneApplyLogCount.fetch_add(1, std::memory_order_relaxed) < 12) {
				logger::info(
					"[OAR-TrackFilter-Camera] Applied {} sampled delta(s) to the post-eval "
					"first-person Camera scene node (evaluation={})",
					deltas.size(), a_evaluation);
			}
		}

		struct PlayerUpdateAnimationHook
		{
			static void Thunk(RE::PlayerCharacter* a_player, float a_delta)
			{
				RestorePreviousContribution(a_player);
				const uint64_t evaluation =
					s_cameraEvaluationSerial.fetch_add(1, std::memory_order_relaxed) + 1;
				s_activeCameraEvaluation.store(evaluation, std::memory_order_release);
				Original(a_player, a_delta);
				s_activeCameraEvaluation.store(0, std::memory_order_release);
				ApplyCurrentContribution(a_player, evaluation);
			}

			static void Install()
			{
				REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::PlayerCharacter[0] };
				Original = vtable.write_vfunc(0x9F, Thunk);
				logger::info(
					"[OAR-TrackFilter-Camera] PlayerCharacter::UpdateAnimation vfunc 0x9F "
					"hook installed (post-evaluation scene-node mode)");
			}

			inline static REL::Relocation<decltype(Thunk)> Original;
		};

		void Install()
		{
			PlayerUpdateAnimationHook::Install();
		}
	}

	void Install()
	{
		ClipGeneratorHooks::Install();
		LoadClipsHooks::Install();
		EnginePatchHooks::Install();
		PreloadHooks::Install();
		UpdateHooks::Install();
		FileRedirectHooks::Install();
		ActionFireEmptyHook::Install();
		SetupSpecialIdleHook::Install();
		AnimGraphEventFeedHook::Install();
		PlayerFireEmptyHook::Install(REL::GetTrampoline());
		AutoReloadSuppression::Install();
		TrackFilterCameraHooks::Install();
		logger::info("[OAR] All hooks installed");
	}

	namespace ClipGeneratorHooks
	{
		void Install()
		{
			auto& vtbl = Offsets::hkbClipGenerator_vtbl;

			_Activate   = reinterpret_cast<ActivateFn>(vtbl.write_vfunc(Offsets::ClipGen_Activate, hkbClipGenerator_Activate));
			_Update     = reinterpret_cast<UpdateFn>(vtbl.write_vfunc(Offsets::ClipGen_Update, hkbClipGenerator_Update));
			_Deactivate = reinterpret_cast<DeactivateFn>(vtbl.write_vfunc(Offsets::ClipGen_Deactivate, hkbClipGenerator_Deactivate));
			_Generate   = reinterpret_cast<GenerateFn>(vtbl.write_vfunc(Offsets::ClipGen_Generate, hkbClipGenerator_Generate));
			_StartEcho  = reinterpret_cast<StartEchoFn>(vtbl.write_vfunc(Offsets::ClipGen_StartEcho, hkbClipGenerator_StartEcho));

			logger::info("[OAR] hkbClipGenerator vtable hooks installed (active mode)");
		}
	}

	namespace LoadClipsHooks
	{
		static bool FuncReferencesAddress(uint8_t* funcStart, int scanLen, uintptr_t funcAddr, uintptr_t targetAddr)
		{
			for (int i = 0; i < scanLen - 6; i++) {
				uint8_t prefix = funcStart[i];
				if (prefix != 0x48 && prefix != 0x4C) continue;

				uint8_t next = funcStart[i + 1];
				if (next != 0x8D && next != 0x8B && next != 0x89) continue;

				uint8_t modrm = funcStart[i + 2];
				if ((modrm & 0xC7) != 0x05) continue;

				int32_t disp = *reinterpret_cast<int32_t*>(funcStart + i + 3);
				uintptr_t resolved = funcAddr + i + 7 + disp;
				if (resolved == targetAddr) return true;
			}
			return false;
		}

		static int CountCallArgRegisters(uint8_t* funcStart, int scanLen)
		{
			int regSetupCount = 0;
			for (int i = 0; i < std::min(scanLen, 64); i++) {
				uint8_t b = funcStart[i];
				if (b == 0x48 || b == 0x4C || b == 0x49) {
					uint8_t next = funcStart[i + 1];
					if (next == 0x89 || next == 0x8B || next == 0x8D) {
						regSetupCount++;
					}
				}
			}
			return regSetupCount;
		}

		void Install()
		{
			// DISABLED ON ALL RUNTIMES (2026-07-28). These heuristic call-site
			// detours are implicated in a family of IO-thread heap-corruption
			// crashes in BShkbHkxDB's EntryDB machinery on BOTH AE and OG:
			//   - AE 1.11.221 (crash-2026-07-28-16-*.log): the scan hooked a
			//     call inside a BSResource entry's scalar_deleting_destructor
			//     (i.e. inside EntryDB<BShkbHkxDB>::FlushReleases' per-entry
			//     destruction) plus a one-argument manager call. Reproducible
			//     equip crash; gone with OAR removed.
			//   - OG 1.10.163 (crash-2026-07-28-22-28-34.log): corrupted
			//     packfile section size inside hkNativePackfileUtils::load
			//     during BShkbHkxDB::hkxDBData::LoadImpl at a weapon workbench
			//     (heavy graph load/release churn). Same session, the hooks
			//     captured ZERO path entries while one-time arg dumps show the
			//     hooked sites firing with non-loadClips signatures (arg1 is a
			//     742655-vtbl object, arg5 path empty, arg4 a small int).
			// Root problem: the scanned call sites are reached from MULTIPLE
			// caller contexts with different signatures (one is the graph
			// class's destructor path). Our thunks are void(6 pointer args);
			// re-issuing the call from the thunk does not preserve stack args
			// beyond the 6th, XMM register args, or the callee's return value,
			// so every invocation from a non-matching context can silently
			// corrupt state on the IO thread. The anchors (REL::ID 742655 /
			// 802975 / 931110) resolve byte-identically to the address library
			// on OG, so the pre-multi-runtime build hooked these same sites;
			// the hazard existed all along and only manifests under churn.
			// Benefit today is zero: s_loadClipsPathMap stays empty (verified
			// 0 entries across a full OG session) and direct-path resolution
			// is fully carried by the subgraph swap-array walk and the polling
			// resolver on every runtime (same session shows full weapon-folder
			// suffixes matching). Consumers of the map all fall back cleanly.
			// The scan below is kept for reference / future RE work only.
			static volatile bool s_enableUnsafeLoadClipsScan = false;
			if (!s_enableUnsafeLoadClipsScan) {
				logger::info("[OAR] LoadClips call-site hooks disabled on all runtimes "
					"(unsafe heuristic sites; path resolution uses subgraph walk + polling)");
				return;
			}

			auto moduleBase = REX::FModule::GetExecutingModule().GetBaseAddress();
			auto textSeg = REX::FModule::GetExecutingModule().GetSection(".text");
			auto textEnd = textSeg.GetAddress() + textSeg.GetSize();

			auto* settings = Settings::GetSingleton();
			auto bsGraphVtblBase = REL::Relocation<uintptr_t>{ REL::ID(742655) }.address();
			auto bindingSetVtbl = REL::Relocation<uintptr_t>{ REL::ID(802975) }.address();
			auto stringDataVtbl = Offsets::hkbCharacterStringData_vtbl.address();

			logger::info("[OAR] Searching for loadClips call sites...");
			logger::info("[OAR]   moduleBase={:X}, BindingSet vtbl={:X}, StringData vtbl={:X}",
				moduleBase, bindingSetVtbl, stringDataVtbl);

			struct Candidate {
				uintptr_t callSite;
				uintptr_t target;
				int vtblIdx;
				int callOffset;
				int score;
			};
			std::vector<Candidate> candidates;

			for (int vi = 0; vi < 40; vi++) {
				auto* vtblEntry = reinterpret_cast<uintptr_t*>(bsGraphVtblBase + vi * 8);
				if (IsBadReadPtr(vtblEntry, 8)) break;
				auto funcAddr = *vtblEntry;
				if (funcAddr < moduleBase || funcAddr >= textEnd) continue;

				auto* funcBytes = reinterpret_cast<uint8_t*>(funcAddr);
				constexpr int funcLimit = 4096;

				for (int off = 0; off < funcLimit; off++) {
					if (IsBadReadPtr(funcBytes + off, 5)) break;
					if (funcBytes[off] != 0xE8) continue;

					int32_t rel = *reinterpret_cast<int32_t*>(funcBytes + off + 1);
					uintptr_t target = funcAddr + off + 5 + rel;
					if (target < moduleBase || target >= textEnd) continue;

					auto* targetBytes = reinterpret_cast<uint8_t*>(target);
					if (IsBadReadPtr(targetBytes, 2048)) continue;

					bool refsBindingSet = FuncReferencesAddress(targetBytes, 2048, target, bindingSetVtbl);
					bool refsStringData = FuncReferencesAddress(targetBytes, 2048, target, stringDataVtbl);

					if (!refsBindingSet && !refsStringData) continue;

					int score = 0;
					if (refsBindingSet) score += 10;
					if (refsStringData) score += 5;

					int regCount = CountCallArgRegisters(funcBytes + std::max(off - 32, 0), 32);
					if (regCount >= 4) score += 5;

					if (settings->bVerboseLogging) {
						logger::info("[OAR]   Candidate: vtbl[{}]+0x{:X} -> {:X} (rva {:X}) score={} {}{}",
							vi, off, target, target - moduleBase, score,
							refsBindingSet ? "[BindingSet] " : "",
							refsStringData ? "[StringData]" : "");
					}

					candidates.push_back({ funcAddr + off, target, vi, off, score });
				}
			}

			std::ranges::sort(candidates, [](const auto& a, const auto& b) {
				return a.score > b.score;
			});

			std::set<uintptr_t> hookedTargets;
			auto& trampoline = REL::GetTrampoline();
			int hookCount = 0;

			for (auto& c : candidates) {
				if (hookedTargets.count(c.target)) continue;
				if (hookCount >= 2) break;

				auto* callSite = reinterpret_cast<uint8_t*>(c.callSite);
				if (IsBadReadPtr(callSite, 5) || *callSite != 0xE8) continue;

				if (hookCount == 0) {
					logger::info("[OAR] Installing loadClips hook #1 at {:X} (target {:X}, rva {:X}) vtbl[{}]+0x{:X}",
						c.callSite, c.target, c.callSite - moduleBase, c.vtblIdx, c.callOffset);
					_LoadClips = reinterpret_cast<LoadClipsFn>(
						trampoline.write_call<5>(c.callSite, reinterpret_cast<uintptr_t>(HookedLoadClips)));
				} else {
					logger::info("[OAR] Installing loadClips hook #2 at {:X} (target {:X}, rva {:X}) vtbl[{}]+0x{:X}",
						c.callSite, c.target, c.callSite - moduleBase, c.vtblIdx, c.callOffset);
					_LoadClips2 = reinterpret_cast<LoadClipsFn>(
						trampoline.write_call<5>(c.callSite, reinterpret_cast<uintptr_t>(HookedLoadClips2)));
				}

				hookedTargets.insert(c.target);
				hookCount++;
				bHookInstalled = true;
			}

			if (hookCount == 0) {
				logger::warn("[OAR] Could not find any loadClips call sites");
				logger::warn("[OAR] Plugin will operate in safe pass-through mode");
			} else {
				logger::info("[OAR] Installed {} loadClips hook(s) covering {} unique target function(s)",
					hookCount, hookedTargets.size());
			}
		}

		bool TryDeferredInjection()
		{
			auto* oar = OpenAnimationReplacer::GetSingleton();
			if (oar->GetTotalReplacementCount() == 0) {
				logger::info("[OAR] TryDeferredInjection: no replacement animations parsed");
				return false;
			}

			logger::info("[OAR] TryDeferredInjection: {} mods loaded, enabling clip hooks for animation swap",
				oar->GetTotalReplacementCount());
			SetHasActiveReplacements(true);

			FileRedirectHooks::BuildFileRedirectMap();

			oar->loadingPhase.store(OpenAnimationReplacer::LoadingPhase::kLoading);
			PreloadReplacementAnimations();

			oar->isLoading.store(false);
			oar->loadingComplete.store(true);

			// Per-line flushing is off (see LogSetup.cpp); make sure the whole
			// load phase is on disk in case anything crashes later.
			if (auto log = spdlog::default_logger()) {
				log->flush();
			}

			return true;

			RE::hkbCharacterStringData* stringData = nullptr;

			{
				std::lock_guard lock(s_capturedMutex);
				if (s_capturedStringData && !IsBadReadPtr(s_capturedStringData, sizeof(uintptr_t))) {
					uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(s_capturedStringData);
					if (vtbl == Offsets::hkbCharacterStringData_vtbl.address()) {
						stringData = s_capturedStringData;
						logger::info("[OAR] TryDeferredInjection: using directly captured stringData");
					}
				}
				s_capturedStringData = nullptr;
			}

			if (!stringData) {
				std::lock_guard lock(s_capturedGraphsMutex);
				logger::info("[OAR] TryDeferredInjection: trying {} captured graph(s)...",
					s_capturedGraphs.size());

				for (auto* graphPtr : s_capturedGraphs) {
					if (!graphPtr || IsBadReadPtr(graphPtr, sizeof(uintptr_t))) continue;

					uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(graphPtr);
					static uintptr_t bsGraphVtbl = REL::Relocation<uintptr_t>{ REL::ID(742655) }.address();
					if (vtbl != bsGraphVtbl) continue;

					auto* sd = ExtractStringDataFromGraph(graphPtr);
					if (sd) {
						stringData = sd;
						logger::info("[OAR] TryDeferredInjection: extracted stringData from graph at {:X}",
							reinterpret_cast<uintptr_t>(graphPtr));
						break;
					}
				}
				s_capturedGraphs.clear();
			}

			if (!stringData) {
				logger::info("[OAR] TryDeferredInjection: trying player's animation graph...");

				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player) {
					logger::warn("[OAR] TryDeferredInjection: PlayerCharacter is null");
				} else {
					RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
					if (!player->GetAnimationGraphManagerImpl(manager) || !manager) {
						logger::warn("[OAR] TryDeferredInjection: no animation graph manager");
					} else {
						logger::info("[OAR] TryDeferredInjection: graph count={}", manager->graph.size());
						for (size_t i = 0; i < manager->graph.size(); i++) {
							auto* graph = manager->graph[i].get();
							if (!graph) continue;
					logger::info("[OAR] TryDeferredInjection: graph[{}] at {:X}", i, (uintptr_t)graph);

						// ---- BEGIN MEMORY PROBE ----
						{
							uintptr_t sdVtblTarget = Offsets::hkbCharacterStringData_vtbl.address();
							auto* graphBytes = reinterpret_cast<uint8_t*>(graph);

							auto* charPtr = reinterpret_cast<RE::hkbCharacter*>(graphBytes + 0x1C8);
							if (!IsBadReadPtr(charPtr, sizeof(void*))) {
								logger::info("[OAR] Probe: character at {:X} (graph+0x1C8)", (uintptr_t)charPtr);

								// Scan character object (0x200 bytes)
								logger::info("[OAR] Probe: scanning character object for stringData vtbl {:X}...", sdVtblTarget);
								auto* charBase = reinterpret_cast<uintptr_t*>(charPtr);
								for (size_t ci = 0; ci < 0x200 / sizeof(uintptr_t); ci++) {
									if (IsBadReadPtr(&charBase[ci], sizeof(uintptr_t))) break;
									uintptr_t val = charBase[ci];
									if (!val || val < 0x10000) continue;
									if (IsBadReadPtr(reinterpret_cast<void*>(val), sizeof(uintptr_t))) continue;
									uintptr_t vtblAt = *reinterpret_cast<uintptr_t*>(val);
									bool match = (vtblAt == sdVtblTarget);
									if (match) {
										logger::info("[OAR] Probe: CHAR offset +0x{:X} = ptr {:X} (vtbl={:X}) [MATCH!]",
											ci * sizeof(uintptr_t), val, vtblAt);
									} else {
										logger::info("[OAR] Probe: CHAR offset +0x{:X} = ptr {:X} (vtbl={:X})",
											ci * sizeof(uintptr_t), val, vtblAt);
									}
								}

								auto* setupFromStruct = charPtr->setup._ptr;
								logger::info("[OAR] Probe: setup via struct = {:X}", (uintptr_t)setupFromStruct);

								auto* setupByOffset = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(charPtr) + 0x78);
								logger::info("[OAR] Probe: setup via raw offset +0x78 = {:X}", (uintptr_t)setupByOffset);

								void* setupToScan = setupFromStruct ? (void*)setupFromStruct : setupByOffset;
								if (setupToScan && !IsBadReadPtr(setupToScan, 0x200)) {
									logger::info("[OAR] Probe: scanning setup object at {:X} (0x200 bytes)...", (uintptr_t)setupToScan);
									auto* setupBase = reinterpret_cast<uintptr_t*>(setupToScan);
									for (size_t si = 0; si < 0x200 / sizeof(uintptr_t); si++) {
										if (IsBadReadPtr(&setupBase[si], sizeof(uintptr_t))) break;
										uintptr_t val = setupBase[si];
										if (!val || val < 0x10000) continue;
										if (IsBadReadPtr(reinterpret_cast<void*>(val), sizeof(uintptr_t))) continue;
										uintptr_t vtblAt = *reinterpret_cast<uintptr_t*>(val);
										bool match = (vtblAt == sdVtblTarget);
										if (match) {
											logger::info("[OAR] Probe: SETUP offset +0x{:X} = ptr {:X} (vtbl={:X}) [MATCH!]",
												si * sizeof(uintptr_t), val, vtblAt);
										} else {
											// Two-level deep: check if this object contains a pointer to stringData
											auto* innerBase = reinterpret_cast<uintptr_t*>(val);
											bool innerMatch = false;
											size_t innerLimit = 0x200 / sizeof(uintptr_t);
											if (!IsBadReadPtr(innerBase, 0x200)) {
												for (size_t ii = 0; ii < innerLimit; ii++) {
													if (IsBadReadPtr(&innerBase[ii], sizeof(uintptr_t))) break;
													uintptr_t innerVal = innerBase[ii];
													if (!innerVal || innerVal < 0x10000) continue;
													if (IsBadReadPtr(reinterpret_cast<void*>(innerVal), sizeof(uintptr_t))) continue;
													uintptr_t innerVtbl = *reinterpret_cast<uintptr_t*>(innerVal);
													if (innerVtbl == sdVtblTarget) {
														logger::info("[OAR] Probe: SETUP offset +0x{:X} -> ptr {:X} (vtbl={:X}) -> inner offset +0x{:X} = {:X} (vtbl={:X}) [INNER MATCH!]",
															si * sizeof(uintptr_t), val, vtblAt,
															ii * sizeof(uintptr_t), innerVal, innerVtbl);
														innerMatch = true;
													}
												}
											}
											if (!innerMatch) {
												logger::info("[OAR] Probe: SETUP offset +0x{:X} = ptr {:X} (vtbl={:X})",
													si * sizeof(uintptr_t), val, vtblAt);
											}
										}
									}
								} else if (setupToScan) {
									logger::info("[OAR] Probe: setup at {:X} is not readable for 0x200 bytes", (uintptr_t)setupToScan);
								} else {
									logger::info("[OAR] Probe: setup is null from both struct and raw offset");
								}
							} else {
								logger::info("[OAR] Probe: character at graph+0x1C8 is unreadable");
							}
						}
						// ---- END MEMORY PROBE ----

						auto* sd = ExtractStringDataFromGraph(graph);
							if (sd) {
								stringData = sd;
								logger::info("[OAR] TryDeferredInjection: extracted stringData from player graph[{}]", i);
								break;
							}

							sd = ScanForStringData(graph, 0x400);
							if (sd) {
								stringData = sd;
								logger::info("[OAR] TryDeferredInjection: found stringData via scan of player graph[{}]", i);
								break;
							}
						}
					}
				}
			}

			if (!stringData) {
				logger::warn("[OAR] TryDeferredInjection: could not find stringData from any source");
				return false;
			}

			logger::info("[OAR] TryDeferredInjection: injecting into stringData at {:X}",
				reinterpret_cast<uintptr_t>(stringData));

			try {
				bool injected = oar->CreateReplacementAnimations("", stringData);
				if (injected) {
					SetHasActiveReplacements(true);
					logger::info("[OAR] TryDeferredInjection: replacement animations injected successfully");
				} else {
					logger::warn("[OAR] TryDeferredInjection: CreateReplacementAnimations returned false (no matches)");
				}
				return injected;
			} catch (const std::exception& e) {
				logger::error("[OAR] TryDeferredInjection exception: {}", e.what());
				return false;
			}
		}
	}

	namespace EnginePatchHooks
	{
		void Install()
		{
			logger::info("[OAR] Engine patches:");

			auto moduleBase = REX::FModule::GetExecutingModule().GetBaseAddress();
			auto textSeg = REX::FModule::GetExecutingModule().GetSection(".text");
			auto textEnd = textSeg.GetAddress() + textSeg.GetSize();

			// movsx→movzx patch on the ORIGINAL Activate function (not our hook)
			// Use the saved original function pointer, not the vtable (which now points to our hook)
			auto originalActivateAddr = reinterpret_cast<uintptr_t>(ClipGeneratorHooks::_Activate);
			if (originalActivateAddr && originalActivateAddr >= moduleBase && originalActivateAddr < textEnd) {
				auto* funcBytes = reinterpret_cast<uint8_t*>(originalActivateAddr);
				bool patched = false;

				for (int off = 0; off < 256; off++) {
					if (IsBadReadPtr(funcBytes + off, 4)) break;

					if (funcBytes[off] == 0x0F && funcBytes[off + 1] == 0xBF) {
						logger::info("[OAR]   Found movsx at original Activate+0x{:X}, patching to movzx", off);
						DWORD oldProtect;
						VirtualProtect(funcBytes + off, 2, PAGE_EXECUTE_READWRITE, &oldProtect);
						funcBytes[off + 1] = 0xB7;
						VirtualProtect(funcBytes + off, 2, oldProtect, &oldProtect);
						patched = true;
						break;
					}
				}
				if (!patched) {
					logger::info("[OAR]   No movsx found in original Activate - may already use unsigned");
				}
			} else {
				logger::info("[OAR]   Skipping movsx patch - original Activate not resolved");
			}

			auto bhkMemVtbl = REL::Relocation<uintptr_t>{ REL::ID(594246) }.address();
			logger::info("[OAR]   bhkThreadMemorySource vtable at {:X}", bhkMemVtbl);

			logger::info("[OAR] Engine patches done");
		}
	}

	namespace PreloadHooks
	{
		void Install()
		{
			logger::info("[OAR] Preload hooks: animation preloading active via LoadClips injection");
		}

		void PreloadReplacementAnimations(RE::BShkbAnimationGraph* a_graph)
		{
			if (!a_graph) return;

			auto* character = &a_graph->character;
			auto* setup = character->setup._ptr;
			if (!setup) return;

			auto* typedSetup = reinterpret_cast<RE::hkbCharacterSetup*>(setup);
			if (!typedSetup->data._ptr) return;

			auto* stringData = typedSetup->data._ptr->stringData._ptr;
			if (!stringData) return;

			auto* projData = OpenAnimationReplacer::GetSingleton()->GetReplacerProjectData(stringData);
			if (!projData) return;

			logger::info("[OAR] Preloading replacement animations for character '{}'",
				character->name.data() ? character->name.data() : "(unknown)");
		}
	}

	namespace UpdateHooks
	{
		void Install()
		{
			if (REX::FModule::IsRuntimeOG()) {
				// OG 1.10.163: trampoline the RunActorUpdates call site
				// (556439 + 0x17, proven from FPInertia). OAR's per-frame work
				// then runs immediately after the game's actor updates.
				auto& trampoline = REL::GetTrampoline();
				RunActorUpdatesOrig = reinterpret_cast<RunActorUpdatesFn>(
					trampoline.write_call<5>(
						Offsets::GetRunActorUpdatesAddr() + Offsets::RunActorUpdates_Offset,
						reinterpret_cast<uintptr_t>(HookedActorUpdate)
					)
				);
				logger::info("[OAR] Actor update hook installed (RunActorUpdates call site)");
				return;
			}

			// NG/AE: 556439 has no Address Library entry in those databases.
			// Drive the per-frame work from F4SE's permanent task queue
			// instead: permanent tasks run on the game thread every frame.
			// RunActorUpdatesOrig stays null; HookedActorUpdate handles that.
			if (const auto* tasks = F4SE::GetTaskInterface()) {
				tasks->AddTaskPermanent([]() { HookedActorUpdate(); });
				logger::info("[OAR] Actor update driver installed (F4SE permanent task, NG/AE path)");
			} else {
				logger::error("[OAR] F4SE task interface unavailable - per-frame polling disabled on this runtime");
			}
		}
	}

	namespace FileRedirectHooks
	{
		using CreateFileW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
		static CreateFileW_t s_origCreateFileW{ nullptr };

		static std::shared_mutex s_fileMapMutex;
		static std::unordered_map<std::string, std::string> s_fileRedirectMap;
		static bool s_fileMapBuilt = false;

		void BuildFileRedirectMap()
		{
			std::unique_lock lock(s_fileMapMutex);

			auto* oar = OpenAnimationReplacer::GetSingleton();
			const auto& pathMap = oar->GetPathToSubModsMap();

			s_fileRedirectMap.clear();

			for (auto& [mapKey, replacementInfos] : pathMap) {
				std::string lowerKey = mapKey;
				std::ranges::transform(lowerKey, lowerKey.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::ranges::replace(lowerKey, '/', '\\');

				for (auto& info : replacementInfos) {
					// Track-filtered submods must NOT redirect files. Their
					// animation files are pose DONORS sampled by the track
					// filter; the game must load the true original untouched.
					// Redirecting them made the engine play the donor content
					// outright, annotations included — a donor holding reload
					// data fired reload sounds AND reload-completion events
					// (ammo refill) the moment the "original" idle played.
					if (info.parentSubMod && info.parentSubMod->trackFilter.enabled) {
						continue;
					}
					// A disabled submod's file must not win the redirect either.
					if (info.parentSubMod && info.parentSubMod->IsDisabled()) {
						continue;
					}
					s_fileRedirectMap[lowerKey] = info.replacementPath;
					if (Settings::GetSingleton()->bVerboseLogging) {
						logger::info("[OAR] FileRedirect: '{}' -> '{}'", lowerKey, info.replacementPath);
					}
					break;
				}
			}

			s_fileMapBuilt = true;
			logger::info("[OAR] File redirect map ready with {} entries", s_fileRedirectMap.size());
		}


		static HANDLE WINAPI HookedCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
			LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
			DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
		{
			if (lpFileName) {
				int len = WideCharToMultiByte(CP_UTF8, 0, lpFileName, -1, nullptr, 0, nullptr, nullptr);
				if (len > 0 && len < 1024) {
					char narrowBuf[1024];
					WideCharToMultiByte(CP_UTF8, 0, lpFileName, -1, narrowBuf, sizeof(narrowBuf), nullptr, nullptr);

					std::string narrow(narrowBuf);
					std::string lower = narrow;
					std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

					if (lower.find(".hkx") != std::string::npos || lower.find(".hkt") != std::string::npos) {
						// ===== Option 2: Capture animation folder from actual file opens =====
						// Extract path relative to "animations\" to build leaf->folder mapping
						auto animPos = lower.find("animations\\");
						if (animPos != std::string::npos) {
							std::string relPath = lower.substr(animPos + 11); // after "animations\"
							// Remove extension
							auto dotPos = relPath.rfind('.');
							if (dotPos != std::string::npos) relPath = relPath.substr(0, dotPos);

							// Split into folder + leaf
							auto lastSlash = relPath.rfind('\\');
							if (lastSlash != std::string::npos && lastSlash > 0) {
								std::string folder = relPath.substr(0, lastSlash);
								std::string leaf = relPath.substr(lastSlash + 1);

								if (!leaf.empty() && !folder.empty()) {
									std::unique_lock capLock(s_createFileAnimMutex);
									s_createFileLeafToFolders[leaf].insert(folder);
									s_createFileLeafToLatestFolder[leaf] = folder;
									s_createFileCapturedPaths.insert(relPath);

									static int s_capLog = 0;
									if (s_capLog < 100) {
										logger::info("[OAR-CreateFile] Captured: '{}' -> folder='{}' leaf='{}'",
											relPath, folder, leaf);
										s_capLog++;
									}
								}
							}
						}

						// File redirect (existing behavior). Honors the global
						// "Enabled" toggle: while disabled the engine must load
						// the VANILLA file, not the replacement.
						if (s_fileMapBuilt && Settings::GetSingleton()->bEnabled) {
							std::shared_lock lock(s_fileMapMutex);

							for (auto& [origSuffix, replacePath] : s_fileRedirectMap) {
								if (lower.find(origSuffix) != std::string::npos) {
									logger::info("[OAR] FILE REDIRECT: '{}' -> '{}'", narrow, replacePath);

									std::wstring wideReplace(replacePath.begin(), replacePath.end());
									lock.unlock();
									return s_origCreateFileW(wideReplace.c_str(), dwDesiredAccess, dwShareMode,
										lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
								}
							}
						}

						static int s_animLogCount = 0;
						if (s_animLogCount < 50) {
							logger::info("[OAR] AnimFile open: '{}'", narrow);
							s_animLogCount++;
						}
					}
				}
			}

			return s_origCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
				lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
		}

		void Install()
		{
			auto* gameModule = GetModuleHandleW(nullptr);
			if (!gameModule) {
				logger::error("[OAR] FileRedirect: failed to get game module");
				return;
			}

			auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(gameModule);
			auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
				reinterpret_cast<uint8_t*>(gameModule) + dosHeader->e_lfanew);
			auto& importDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

			if (!importDir.VirtualAddress) {
				logger::error("[OAR] FileRedirect: no import directory");
				return;
			}

			auto* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
				reinterpret_cast<uint8_t*>(gameModule) + importDir.VirtualAddress);

			auto* realCreateFileW = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "CreateFileW");
			bool hooked = false;

			for (; importDesc->Name != 0; importDesc++) {
				auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
					reinterpret_cast<uint8_t*>(gameModule) + importDesc->FirstThunk);

				for (; thunk->u1.Function != 0; thunk++) {
					if (reinterpret_cast<void*>(thunk->u1.Function) == realCreateFileW) {
						s_origCreateFileW = reinterpret_cast<CreateFileW_t>(thunk->u1.Function);

						DWORD oldProtect;
						VirtualProtect(&thunk->u1.Function, sizeof(uintptr_t), PAGE_READWRITE, &oldProtect);
						thunk->u1.Function = reinterpret_cast<uintptr_t>(HookedCreateFileW);
						VirtualProtect(&thunk->u1.Function, sizeof(uintptr_t), oldProtect, &oldProtect);

						hooked = true;
						break;
					}
				}
				if (hooked) break;
			}

			if (hooked) {
				logger::info("[OAR] CreateFileW IAT hook installed (file redirect active)");
			} else {
				s_origCreateFileW = reinterpret_cast<CreateFileW_t>(realCreateFileW);
				logger::warn("[OAR] CreateFileW IAT entry not found, file redirect disabled");
			}
		}
	}
}

// =============================================================================
// Clip query collectors — back the external Clips API (RequestPluginAPI_Clips).
//
// Defined at the end of the TU so every file-static map/mutex and helper
// (suffix/path caches, active submod map, perspective classifier, validated
// original lookup) declared above is in scope. MAIN THREAD ONLY: the graph
// walk reads live Havok structures exactly like PollPlayerGraphClips does,
// with the same vtable + IsBadReadPtr guards.
// =============================================================================

size_t CollectActorClipQueryData(RE::TESObjectREFR* a_refr, std::vector<OARClipQueryData>& a_out)
{
	a_out.clear();
	if (!a_refr) return 0;

	RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
	if (!a_refr->GetAnimationGraphManagerImpl(manager) || !manager) return 0;

	// Same layout constants as PollPlayerGraphClips (verified against GunMover).
	constexpr uintptr_t kBShkb_HkRootGraph = 0x378;
	constexpr uintptr_t kBG_ActiveNodes = 0xE0;

	static REL::Relocation<uintptr_t> bshkbVtbl{ RE::VTABLE::BShkbAnimationGraph[0] };
	const auto clipVtbl = Offsets::hkbClipGenerator_vtbl.address();
	const uint32_t formID = a_refr->GetFormID();

	auto* cache = AnimationCache::GetSingleton();

	// One tracker snapshot for replacement-path lookups (keyed actorID+suffix).
	const auto replacementSnapshot = ActiveReplacementTracker::GetSingleton()->GetSnapshot();

	for (uint32_t gi = 0; gi < manager->graph.size() && gi < 4; ++gi) {
		const auto root = reinterpret_cast<uintptr_t>(manager->graph[gi].get());
		if (!root || root < 0x10000 ||
			IsBadReadPtr(reinterpret_cast<void*>(root), kBShkb_HkRootGraph + 8) ||
			*reinterpret_cast<uintptr_t*>(root) != bshkbVtbl.address()) {
			continue;
		}
		const auto hkGraph = *reinterpret_cast<uintptr_t*>(root + kBShkb_HkRootGraph);
		if (!hkGraph || hkGraph < 0x10000 ||
			IsBadReadPtr(reinterpret_cast<void*>(hkGraph), 0x1B0)) {
			continue;
		}
		// Skip while the graph rebuilds its node list (same gate as the poll).
		if (*reinterpret_cast<const uint8_t*>(hkGraph + 0x1AC) != 0 ||
			*reinterpret_cast<const uint8_t*>(hkGraph + 0x1AD) != 0) {
			continue;
		}
		const auto activeNodes = *reinterpret_cast<uintptr_t*>(hkGraph + kBG_ActiveNodes);
		if (!activeNodes || IsBadReadPtr(reinterpret_cast<void*>(activeNodes), 0x10)) {
			continue;
		}
		const auto data = *reinterpret_cast<uintptr_t*>(activeNodes);
		const auto size = *reinterpret_cast<int32_t*>(activeNodes + 8);
		if (!data || size <= 0 || size > 0x1000 ||
			IsBadReadPtr(reinterpret_cast<void*>(data), static_cast<size_t>(size) * sizeof(void*))) {
			continue;
		}

		for (int32_t i = 0; i < size; ++i) {
			const auto entry = *reinterpret_cast<uintptr_t*>(data + static_cast<uintptr_t>(i) * sizeof(void*));
			if (!entry || IsBadReadPtr(reinterpret_cast<void*>(entry), 0x18)) {
				continue;
			}

			// Node entry itself, or entry+0x08, whichever carries the clip vtable.
			uintptr_t clipAddr = 0;
			if (*reinterpret_cast<uintptr_t*>(entry) == clipVtbl) {
				clipAddr = entry;
			} else {
				const auto candidate = *reinterpret_cast<uintptr_t*>(entry + 0x08);
				if (candidate && candidate > 0x10000 &&
					!IsBadReadPtr(reinterpret_cast<void*>(candidate), sizeof(void*)) &&
					*reinterpret_cast<uintptr_t*>(candidate) == clipVtbl) {
					clipAddr = candidate;
				}
			}
			if (!clipAddr) continue;

			auto* clip = reinterpret_cast<RE::hkbClipGenerator*>(clipAddr);

			auto& d = a_out.emplace_back();
			d.clipHandle = clipAddr;
			d.actorFormID = formID;
			d.graphIndex = static_cast<uint8_t>(gi);
			d.playbackMode = static_cast<uint8_t>(clip->mode);
			d.playbackSpeed = clip->playbackSpeed;

			// Authored animation path — may be a template (e.g. "44pistol\...")
			// until the subgraph resolution provides the real directory.
			if (const char* an = clip->animationName.data();
				an && reinterpret_cast<uintptr_t>(an) > 0x10000 && !IsBadReadPtr(an, 1)) {
				d.animationName = an;
			}

			// Runtime playback state exists only after activation.
			if (clip->GetAnimationControlRaw()) {
				d.localTime = clip->GetLocalTime();
				if (auto* anim = clip->GetAnimation(); anim && !IsBadReadPtr(anim, 0x20)) {
					d.duration = anim->duration;
					if (cache->IsOurReplacement(anim)) {
						d.replacementKind = 1;  // full-body swap in the slot
						if (auto* orig = GetValidOriginal(clip)) {
							d.originalDuration = orig->duration;
						}
					} else {
						d.originalDuration = d.duration;
					}
				}
			}

			// OAR-resolved data from the per-clip caches.
			{
				std::shared_lock lock(s_clipSuffixMutex);
				auto it = s_clipSuffixCache.find(clip);
				if (it != s_clipSuffixCache.end()) d.suffix = it->second;
			}
			{
				std::shared_lock lock(s_clipRealPathMutex);
				auto it = s_clipRealPathCache.find(clip);
				if (it != s_clipRealPathCache.end()) d.resolvedPath = it->second;
			}
			d.perspective = static_cast<uint8_t>(ClassifyClipPerspective(clip, d.resolvedPath));

			// Active replacement attribution (also set for track-filtered clips,
			// whose slot still holds the original animation).
			SubMod* activeSubMod = nullptr;
			{
				std::shared_lock smLock(s_activeSubModMutex);
				auto it = s_activeSubModMap.find(clip);
				if (it != s_activeSubModMap.end()) activeSubMod = it->second;
			}
			if (activeSubMod) {
				d.subModName = activeSubMod->GetName();
				d.subModPriority = activeSubMod->GetPriority();
				if (d.replacementKind == 0 && activeSubMod->trackFilter.enabled) {
					d.replacementKind = 2;  // partial-bone override, no slot swap
				}
				// Parent replacer mod: search the registry (SubMod stores no
				// back-pointer). Registry is small; this runs on demand only.
				auto* oar = OpenAnimationReplacer::GetSingleton();
				std::shared_lock modsLock(oar->GetModsMutex());
				for (const auto& mod : oar->GetReplacerMods()) {
					bool found = false;
					for (const auto& sub : mod->GetSubMods()) {
						if (sub.get() == activeSubMod) {
							d.modName = mod->GetName();
							found = true;
							break;
						}
					}
					if (found) break;
				}
				// Replacement file path from the tracker (keyed actor+suffix).
				for (const auto& rep : replacementSnapshot) {
					if (rep.actorFormID == formID && rep.clipSuffix == d.suffix) {
						d.replacementPath = rep.replacementPath;
						break;
					}
				}
			}
		}
	}

	return a_out.size();
}

size_t CollectClipAnnotations(uintptr_t a_clipHandle, std::vector<std::pair<float, std::string>>& a_out)
{
	a_out.clear();

	auto* clip = reinterpret_cast<RE::hkbClipGenerator*>(a_clipHandle);
	if (!IsLiveClipGenerator(clip)) return 0;

	auto* anim = clip->GetAnimation();
	if (!anim || IsBadReadPtr(anim, 0x40)) return 0;

	// Replaced clip: the clone's embedded annotation tracks are deliberately
	// nulled (OAR fires annotations manually), so read the parsed annotations
	// from the cache — the same list the manual firing uses.
	auto* cache = AnimationCache::GetSingleton();
	if (cache->IsOurReplacement(anim)) {
		std::string suffix;
		{
			std::shared_lock lock(s_clipSuffixMutex);
			auto it = s_clipSuffixCache.find(clip);
			if (it != s_clipSuffixCache.end()) suffix = it->second;
		}
		if (suffix.empty()) return 0;
		SubMod* owner = nullptr;
		{
			std::shared_lock smLock(s_activeSubModMutex);
			auto it = s_activeSubModMap.find(clip);
			if (it != s_activeSubModMap.end()) owner = it->second;
		}
		if (const auto* annots = cache->GetAnnotations(suffix, owner)) {
			for (const auto& a : *annots) {
				a_out.emplace_back(a.time, a.text);
			}
		}
		return a_out.size();
	}

	// Game animation: parse the raw hkaAnnotationTrack array (same guarded
	// offsets as the Activate-time original-annotation cache: tracks at
	// anim+0x28/count +0x30; per track annotations ptr +0x08 / count +0x10;
	// per annotation time +0x00 / text +0x08 with the low flag bit masked).
	auto* animBytes = reinterpret_cast<uint8_t*>(anim);
	auto* trackPtr = *reinterpret_cast<uint8_t**>(animBytes + 0x28);
	int32_t trackCount = *reinterpret_cast<int32_t*>(animBytes + 0x30);
	if (!trackPtr || trackCount <= 0 || trackCount > 0x200 ||
		reinterpret_cast<uintptr_t>(trackPtr) < 0x10000 ||
		IsBadReadPtr(trackPtr, static_cast<size_t>(trackCount) * 0x18)) {
		return 0;
	}

	constexpr size_t kAnnotTrackSize = 0x18;
	constexpr size_t kAnnotationSize = 0x10;

	for (int32_t t = 0; t < trackCount; ++t) {
		auto* trackBase = trackPtr + (t * kAnnotTrackSize);
		auto* annots = *reinterpret_cast<uint8_t**>(trackBase + 0x08);
		int32_t annotCount = *reinterpret_cast<int32_t*>(trackBase + 0x10);
		if (!annots || annotCount <= 0 || annotCount > 0x1000 ||
			reinterpret_cast<uintptr_t>(annots) < 0x10000 ||
			IsBadReadPtr(annots, static_cast<size_t>(annotCount) * kAnnotationSize)) {
			continue;
		}
		for (int32_t a = 0; a < annotCount; ++a) {
			auto* annBase = annots + (a * kAnnotationSize);
			float time = *reinterpret_cast<float*>(annBase + 0x00);
			auto rawTxt = *reinterpret_cast<uintptr_t*>(annBase + 0x08) & ~uintptr_t(1);
			auto* txt = reinterpret_cast<const char*>(rawTxt);
			if (txt && rawTxt > 0x10000 && !IsBadReadPtr(txt, 1) && txt[0] != '\0') {
				a_out.emplace_back(time, std::string(txt));
			}
		}
	}

	return a_out.size();
}

// =============================================================================
// Graph query collectors — back the Clips API v2 graph-level queries.
// Same safety model as the clip collectors: every hop through live Havok
// structures is vtable- and IsBadReadPtr-guarded. MAIN THREAD ONLY.
// =============================================================================

namespace
{
	// BShkbAnimationGraph layout constants shared by the graph queries
	// (identical to the locals used by PollPlayerGraphClips / the clip collector).
	constexpr uintptr_t kQuery_HkRootGraph = 0x378;   // BShkb -> hkbBehaviorGraph
	constexpr uintptr_t kQuery_ActiveNodes = 0xE0;    // hkbBehaviorGraph -> active node array

	// Validates manager->graph[gi] as a live BShkbAnimationGraph. Returns the
	// root address or 0.
	uintptr_t QueryValidateGraphRoot(RE::BSAnimationGraphManager* a_manager, uint32_t a_gi)
	{
		static REL::Relocation<uintptr_t> bshkbVtbl{ RE::VTABLE::BShkbAnimationGraph[0] };
		if (a_gi >= a_manager->graph.size()) return 0;
		const auto root = reinterpret_cast<uintptr_t>(a_manager->graph[a_gi].get());
		if (!root || root < 0x10000 ||
			IsBadReadPtr(reinterpret_cast<void*>(root), kQuery_HkRootGraph + 8) ||
			*reinterpret_cast<uintptr_t*>(root) != bshkbVtbl.address()) {
			return 0;
		}
		return root;
	}

	// The graph's embedded hkbCharacter (at +0x1C8), or nullptr when unreadable.
	RE::hkbCharacter* QueryGraphCharacter(uintptr_t a_root)
	{
		auto* character = reinterpret_cast<RE::hkbCharacter*>(a_root + kGraph_EmbeddedCharacter);
		if (IsBadReadPtr(character, sizeof(RE::hkbCharacter))) return nullptr;
		return character;
	}

	// Guarded hkStringPtr-style read: masks the low ownership-flag bit and
	// validates the pointer before treating it as a C string.
	std::string QueryReadHkString(uintptr_t a_rawPtrValue)
	{
		const auto masked = a_rawPtrValue & ~uintptr_t(1);
		auto* str = reinterpret_cast<const char*>(masked);
		if (!str || masked < 0x10000 || IsBadReadPtr(str, 1) || str[0] == '\0') return {};
		return std::string(str);
	}

	// character -> setup -> animationSkeleton, with guards. Returns nullptr
	// when any hop is unreadable.
	uint8_t* QueryGraphSkeleton(RE::hkbCharacter* a_character)
	{
		auto* setup = a_character->setup._ptr;
		if (!setup || reinterpret_cast<uintptr_t>(setup) < 0x10000 || IsBadReadPtr(setup, 0x50)) return nullptr;
		auto* skeleton = reinterpret_cast<uint8_t*>(setup->animationSkeleton._ptr);
		if (!skeleton || reinterpret_cast<uintptr_t>(skeleton) < 0x10000 || IsBadReadPtr(skeleton, 0x40)) return nullptr;
		return skeleton;
	}

	// character -> setup -> data -> stringData (hkbCharacterStringData), guarded.
	RE::hkbCharacterStringData* QueryGraphStringData(RE::hkbCharacter* a_character)
	{
		auto* setup = a_character->setup._ptr;
		if (!setup || reinterpret_cast<uintptr_t>(setup) < 0x10000 || IsBadReadPtr(setup, 0x50)) return nullptr;
		auto* data = setup->data._ptr;
		if (!data || reinterpret_cast<uintptr_t>(data) < 0x10000 || IsBadReadPtr(data, 0xC0)) return nullptr;
		auto* stringData = data->stringData._ptr;
		if (!stringData || reinterpret_cast<uintptr_t>(stringData) < 0x10000 || IsBadReadPtr(stringData, 0x80)) return nullptr;
		return stringData;
	}

	// character -> projectData -> stringData (hkbProjectStringData), guarded.
	RE::hkbProjectStringData* QueryGraphProjectStrings(RE::hkbCharacter* a_character)
	{
		auto* projData = a_character->projectData._ptr;
		if (!projData || reinterpret_cast<uintptr_t>(projData) < 0x10000 || IsBadReadPtr(projData, 0x30)) return nullptr;
		auto* projStrData = projData->stringData._ptr;
		if (!projStrData || reinterpret_cast<uintptr_t>(projStrData) < 0x10000 || IsBadReadPtr(projStrData, 0x80)) return nullptr;
		return projStrData;
	}
}

size_t CollectActorGraphQueryData(RE::TESObjectREFR* a_refr, std::vector<OARGraphQueryData>& a_out)
{
	a_out.clear();
	if (!a_refr) return 0;

	RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
	if (!a_refr->GetAnimationGraphManagerImpl(manager) || !manager) return 0;

	const auto clipVtbl = Offsets::hkbClipGenerator_vtbl.address();
	const uint32_t formID = a_refr->GetFormID();
	const bool isPlayer = (a_refr == RE::PlayerCharacter::GetSingleton());

	for (uint32_t gi = 0; gi < manager->graph.size() && gi < 4; ++gi) {
		const auto root = QueryValidateGraphRoot(manager.get(), gi);
		if (!root) continue;

		auto& d = a_out.emplace_back();
		d.actorFormID = formID;
		d.graphIndex = static_cast<uint8_t>(gi);
		d.isFirstPerson = isPlayer &&
			s_firstPersonGraphIndex.load(std::memory_order_relaxed) == static_cast<int32_t>(gi);

		// Behavior graph state: rebuild flags + active node/clip counts.
		const auto hkGraph = *reinterpret_cast<uintptr_t*>(root + kQuery_HkRootGraph);
		if (hkGraph && hkGraph > 0x10000 &&
			!IsBadReadPtr(reinterpret_cast<void*>(hkGraph), 0x1B0)) {
			d.isRebuilding = *reinterpret_cast<const uint8_t*>(hkGraph + 0x1AC) != 0 ||
				*reinterpret_cast<const uint8_t*>(hkGraph + 0x1AD) != 0;

			const auto activeNodes = *reinterpret_cast<uintptr_t*>(hkGraph + kQuery_ActiveNodes);
			if (activeNodes && !IsBadReadPtr(reinterpret_cast<void*>(activeNodes), 0x10)) {
				const auto nodeData = *reinterpret_cast<uintptr_t*>(activeNodes);
				const auto nodeSize = *reinterpret_cast<int32_t*>(activeNodes + 8);
				if (nodeData && nodeSize > 0 && nodeSize <= 0x1000 &&
					!IsBadReadPtr(reinterpret_cast<void*>(nodeData), static_cast<size_t>(nodeSize) * sizeof(void*))) {
					d.activeNodeCount = static_cast<uint32_t>(nodeSize);
					for (int32_t i = 0; i < nodeSize; ++i) {
						const auto entry = *reinterpret_cast<uintptr_t*>(nodeData + static_cast<uintptr_t>(i) * sizeof(void*));
						if (!entry || IsBadReadPtr(reinterpret_cast<void*>(entry), 0x18)) continue;
						if (*reinterpret_cast<uintptr_t*>(entry) == clipVtbl) {
							d.activeClipCount++;
						} else {
							const auto candidate = *reinterpret_cast<uintptr_t*>(entry + 0x08);
							if (candidate && candidate > 0x10000 &&
								!IsBadReadPtr(reinterpret_cast<void*>(candidate), sizeof(void*)) &&
								*reinterpret_cast<uintptr_t*>(candidate) == clipVtbl) {
								d.activeClipCount++;
							}
						}
					}
				}
			}
		}

		auto* character = QueryGraphCharacter(root);
		if (!character) continue;

		// Havok character name (hkStringPtr member — read raw to mask the flag bit).
		d.characterName = QueryReadHkString(
			*reinterpret_cast<const uintptr_t*>(reinterpret_cast<const uint8_t*>(&character->name)));

		// Skeleton bone count.
		if (auto* skeleton = QueryGraphSkeleton(character)) {
			auto* bonesArr = reinterpret_cast<RE::hkArrayRawLayout*>(skeleton + RE::kSkeletonOffset_bones);
			if (bonesArr->data && bonesArr->size > 0 && bonesArr->size < 0x1000) {
				d.boneCount = static_cast<uint32_t>(bonesArr->size);
			}
		}

		// Registered animation path count (character string data).
		if (auto* stringData = QueryGraphStringData(character)) {
			auto* arrBase = reinterpret_cast<const uint8_t*>(&stringData->animationNames);
			const int32_t nameSize = *reinterpret_cast<const int32_t*>(arrBase + 8);
			if (nameSize > 0 && nameSize < 0x4000) {
				d.animationNameCount = static_cast<uint32_t>(nameSize);
			}
		}

		// Project strings: animation/behavior roots + event name count.
		if (auto* projStrData = QueryGraphProjectStrings(character)) {
			d.projectAnimationPath = QueryReadHkString(
				*reinterpret_cast<const uintptr_t*>(reinterpret_cast<const uint8_t*>(&projStrData->animationPath)));
			d.behaviorPath = QueryReadHkString(
				*reinterpret_cast<const uintptr_t*>(reinterpret_cast<const uint8_t*>(&projStrData->behaviorPath)));
			auto* evBase = reinterpret_cast<const uint8_t*>(&projStrData->eventNames);
			const int32_t evSize = *reinterpret_cast<const int32_t*>(evBase + 8);
			if (evSize > 0 && evSize < 0x4000) {
				d.eventNameCount = static_cast<uint32_t>(evSize);
			}
		}
	}

	return a_out.size();
}

size_t CollectGraphBones(RE::TESObjectREFR* a_refr, uint32_t a_graphIndex, std::vector<OARBoneQueryData>& a_out)
{
	a_out.clear();
	if (!a_refr) return 0;

	RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
	if (!a_refr->GetAnimationGraphManagerImpl(manager) || !manager) return 0;

	const auto root = QueryValidateGraphRoot(manager.get(), a_graphIndex);
	if (!root) return 0;
	auto* character = QueryGraphCharacter(root);
	if (!character) return 0;
	auto* skeleton = QueryGraphSkeleton(character);
	if (!skeleton) return 0;

	auto* bonesArr = reinterpret_cast<RE::hkArrayRawLayout*>(skeleton + RE::kSkeletonOffset_bones);
	auto* parentArr = reinterpret_cast<RE::hkArrayRawLayout*>(skeleton + RE::kSkeletonOffset_parentIndices);
	if (!bonesArr->data || bonesArr->size <= 0 || bonesArr->size >= 0x1000) return 0;
	if (IsBadReadPtr(bonesArr->data, static_cast<size_t>(bonesArr->size) * RE::kHkaBoneStride)) return 0;

	const int32_t numBones = bonesArr->size;
	auto* boneData = reinterpret_cast<uint8_t*>(bonesArr->data);
	const int16_t* parents = nullptr;
	if (parentArr->data && parentArr->size >= numBones &&
		!IsBadReadPtr(parentArr->data, static_cast<size_t>(numBones) * sizeof(int16_t))) {
		parents = reinterpret_cast<const int16_t*>(parentArr->data);
	}

	a_out.reserve(static_cast<size_t>(numBones));
	for (int32_t i = 0; i < numBones; ++i) {
		auto& b = a_out.emplace_back();
		b.index = static_cast<int16_t>(i);
		b.parentIndex = parents ? parents[i] : int16_t(-1);
		b.name = QueryReadHkString(*reinterpret_cast<uintptr_t*>(boneData + i * RE::kHkaBoneStride));
	}

	return a_out.size();
}

size_t CollectGraphAnimationNames(RE::TESObjectREFR* a_refr, uint32_t a_graphIndex, std::vector<std::string>& a_out)
{
	a_out.clear();
	if (!a_refr) return 0;

	RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
	if (!a_refr->GetAnimationGraphManagerImpl(manager) || !manager) return 0;

	const auto root = QueryValidateGraphRoot(manager.get(), a_graphIndex);
	if (!root) return 0;
	auto* character = QueryGraphCharacter(root);
	if (!character) return 0;
	auto* stringData = QueryGraphStringData(character);
	if (!stringData) return 0;

	// hkArray<FileNameMeshNamePair>: data ptr +0, size +8; pair = 2 hkStringPtrs.
	auto* arrBase = reinterpret_cast<const uint8_t*>(&stringData->animationNames);
	auto* nameData = *reinterpret_cast<uint8_t* const*>(arrBase);
	const int32_t nameSize = *reinterpret_cast<const int32_t*>(arrBase + 8);
	if (!nameData || nameSize <= 0 || nameSize >= 0x4000 ||
		IsBadReadPtr(nameData, static_cast<size_t>(nameSize) * 0x10)) {
		return 0;
	}

	a_out.reserve(static_cast<size_t>(nameSize));
	for (int32_t i = 0; i < nameSize; ++i) {
		// fileName is the first hkStringPtr of the pair.
		a_out.push_back(QueryReadHkString(*reinterpret_cast<const uintptr_t*>(nameData + i * 0x10)));
	}

	return a_out.size();
}

size_t CollectGraphEventNames(RE::TESObjectREFR* a_refr, uint32_t a_graphIndex, std::vector<std::string>& a_out)
{
	a_out.clear();
	if (!a_refr) return 0;

	RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
	if (!a_refr->GetAnimationGraphManagerImpl(manager) || !manager) return 0;

	const auto root = QueryValidateGraphRoot(manager.get(), a_graphIndex);
	if (!root) return 0;
	auto* character = QueryGraphCharacter(root);
	if (!character) return 0;
	auto* projStrData = QueryGraphProjectStrings(character);
	if (!projStrData) return 0;

	// hkArray<hkStringPtr>: data ptr +0, size +8; each element is one hkStringPtr.
	auto* evBase = reinterpret_cast<const uint8_t*>(&projStrData->eventNames);
	auto* evData = *reinterpret_cast<uint8_t* const*>(evBase);
	const int32_t evSize = *reinterpret_cast<const int32_t*>(evBase + 8);
	if (!evData || evSize <= 0 || evSize >= 0x4000 ||
		IsBadReadPtr(evData, static_cast<size_t>(evSize) * sizeof(void*))) {
		return 0;
	}

	a_out.reserve(static_cast<size_t>(evSize));
	for (int32_t i = 0; i < evSize; ++i) {
		a_out.push_back(QueryReadHkString(*reinterpret_cast<const uintptr_t*>(evData + i * sizeof(void*))));
	}

	return a_out.size();
}
