#include "Hooks.h"
#include "Offsets.h"
#include "HavokTypes.h"
#include "Settings.h"
#include "ActiveClip.h"
#include "OpenAnimationReplacer.h"
#include "ReplacerMods.h"
#include "AnimationCache.h"
#include "AnimationLog.h"
#include "ActiveReplacementTracker.h"
#include "RE_Additions.h"
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

	// Cache rep/base pose by bone NAME so it's portable across 1p/3p skeletons.
	std::unordered_map<std::string, RE::hkQsTransformRaw> cachedRepByName;
	std::unordered_map<std::string, RE::hkQsTransformRaw> cachedBaseByName;
	bool cacheValid = false;

	// Per-character bone-name → index resolution (rebuilt when filter version changes).
	std::unordered_map<RE::hkbCharacter*, CharResolved> resolvedByChar;

	// Wall-clock time (seconds, from s_tfTimeSec) of the last source-clip
	// Generate. Used to detect that source clips stopped generating (without
	// firing a Deactivate hook). Time-based, NOT frame-based, so the staleness
	// window is identical at any framerate.
	float lastSourceTimeSec = 0.0f;

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
static std::shared_mutex s_playOnceDecisionMutex;
static std::unordered_map<RE::hkbClipGenerator*, bool> s_playOnceDecision;

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

static RE::TESObjectREFR* GetRefrFromCharacter(RE::hkbCharacter* a_char) {
	if (!a_char) return nullptr;
	std::shared_lock lock(s_characterCacheMutex);
	auto it = s_characterCache.find(a_char);
	return (it != s_characterCache.end()) ? it->second : nullptr;
}

// ---- Easing for temporal blend ----

static float EaseInOutQuad(float t)
{
	if (t <= 0.0f) return 0.0f;
	if (t >= 1.0f) return 1.0f;
	return t < 0.5f ? 2.0f * t * t : t * (4.0f - 2.0f * t) - 1.0f;
}

// ---- Quaternion math helpers for bone blending ----

static float QuatDot(const float* a, const float* b)
{
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
}

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
	bool includeChildren;
	bool excludeChildren;
	{
		std::lock_guard lock(a_filter->boneMutex);
		wantedNames = a_filter->boneNames;
		includeChildren = a_filter->includeChildren;
		excludeNames = a_filter->excludeBoneNames;
		excludeChildren = a_filter->excludeChildren;
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

	// Track the active SubMod per clip for firing custom "on end" events at deactivation.
	static std::shared_mutex s_activeSubModMutex;
	static std::unordered_map<RE::hkbClipGenerator*, SubMod*> s_activeSubModMap;

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

		// Verify vtable matches the game's known hkaAnimation vtable (exact match, not range)
		auto vtbl = *reinterpret_cast<uintptr_t*>(candidate);
		auto expectedVtbl = AnimationCache::GetSingleton()->GetGameAnimVtable();
		if (expectedVtbl != 0 && vtbl != expectedVtbl) {
			static int s_vtblLog = 0;
			if (s_vtblLog < 30) {
				logger::warn("[OAR-ValidOrig] originalAnim={:X} vtbl={:X} != expected={:X} for clipGen={:X} — erasing",
					reinterpret_cast<uintptr_t>(candidate), vtbl, expectedVtbl,
					reinterpret_cast<uintptr_t>(a_clip));
				s_vtblLog++;
			}
			olock.unlock();
			std::unique_lock wlock(s_originalAnimMutex);
			s_originalAnimMap.erase(a_clip);
			return nullptr;
		}

		// Fallback range check when exact vtable not yet captured
		if (expectedVtbl == 0 && (vtbl < 0x7FF000000000ull || vtbl > 0x7FFF00000000ull)) {
			static int s_rangeLog = 0;
			if (s_rangeLog < 30) {
				logger::warn("[OAR-ValidOrig] originalAnim={:X} vtbl={:X} out of range for clipGen={:X} — erasing",
					reinterpret_cast<uintptr_t>(candidate), vtbl, reinterpret_cast<uintptr_t>(a_clip));
				s_rangeLog++;
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
	struct TriggersBackup
	{
		void* triggers{ nullptr };          // raw hkRefPtr value (stored as void* — lifetime managed by Havok refcount)
		void* originalTriggers{ nullptr };
		bool nulled{ false };
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

	// NULL the clip generator's triggers to prevent the engine from firing the ORIGINAL
	// animation's annotation events.  The clone's annotationTracks are zeroed (size=0),
	// so the engine cannot rebuild triggers from them.  We fire replacement annotations
	// manually via the dual-path emission system below.
	static void InstallReplacementTriggers(RE::hkbClipGenerator* a_clipGen, const std::string& /*a_replacementSuffix*/)
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
			static int s_installLog = 0;
			if (s_installLog < 30) {
				logger::info("[OAR-Triggers] NULL'd clipGen={:X} orig triggers={:X}/{:X}",
					reinterpret_cast<uintptr_t>(a_clipGen),
					reinterpret_cast<uintptr_t>(backup.triggers),
					reinterpret_cast<uintptr_t>(backup.originalTriggers));
				s_installLog++;
			}
		}

		*triggersPtr = nullptr;
		*origTriggersPtr = nullptr;
	}

	// Every frame: ensure triggers stay NULL'd (engine may restore originals between frames).
	static void EnsureReplacementTriggersInstalled(RE::hkbClipGenerator* a_clipGen, const std::string& /*a_replacementSuffix*/)
	{
		if (!a_clipGen) return;
		auto* bytes = reinterpret_cast<uint8_t*>(a_clipGen);
		auto* triggersPtr = reinterpret_cast<void**>(bytes + kClipGenTriggersOffset);
		auto* origTriggersPtr = reinterpret_cast<void**>(bytes + kClipGenOriginalTriggersOffset);

		if (!*triggersPtr && !*origTriggersPtr) return;

		std::unique_lock lock(s_triggersBackupMutex);
		auto& backup = s_triggersBackup[a_clipGen];
		if (!backup.nulled) {
			backup.triggers = *triggersPtr;
			backup.originalTriggers = *origTriggersPtr;
			backup.nulled = true;
		}
		*triggersPtr = nullptr;
		*origTriggersPtr = nullptr;
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

	static void PlaySoundDirect(const char* a_soundName, RE::TESObjectREFR* a_refr)
	{
		if (!a_soundName || !a_soundName[0]) return;

		static REL::Relocation<void**> s_audioMgrPtr{ REL::ID(1321158) };
		void* audioMgr = *s_audioMgrPtr;
		if (!audioMgr) return;

		using GetSoundByName_t = void(*)(void* mgr, OARSoundHandle* handle, const char* name,
			float distance, uint32_t usageFlags, void* extraData);
		static REL::Relocation<GetSoundByName_t> GetSoundByName{ REL::ID(196484) };

		using FadeInPlay_t = bool(*)(OARSoundHandle* handle, uint16_t ms);
		static REL::Relocation<FadeInPlay_t> FadeInPlay{ REL::ID(353528) };

		OARSoundHandle handle{};
		GetSoundByName(audioMgr, &handle, a_soundName, 0.f, 0x1A, nullptr);

		if (handle.soundID == 0 && !handle.assumeSuccess) {
			static int s_failLog = 0;
			if (s_failLog < 30) {
				logger::info("[OAR-Audio] GetSoundHandleByName('{}') failed — no sound descriptor found", a_soundName);
				s_failLog++;
			}
			return;
		}

		FadeInPlay(&handle, 0);

		static int s_playLog = 0;
		if (s_playLog < 50) {
			logger::info("[OAR-Audio] Played sound '{}' (id={:X}) on {:X}",
				a_soundName, handle.soundID,
				a_refr ? a_refr->GetFormID() : 0u);
			s_playLog++;
		}
	}

	// Dual-path emission: fire directly to BSTEventSource sinks (audio system, etc.)
	static void NotifyEventSinks(RE::TESObjectREFR* a_refr, const RE::BSFixedString& a_evt)
	{
		RE::BSScrapArray<RE::BSTEventSource<RE::BSAnimationGraphEvent>*> sources;
		if (!RE::BGSAnimationSystemUtils::GetEventSourcePointersFromGraph(a_refr, sources)) return;
		static const RE::BSFixedString emptyArg{ "" };
		for (auto* src : sources) {
			if (!src) continue;
			RE::BSAnimationGraphEvent ge{};
			ge.refr = a_refr;
			ge.animEvent = a_evt;
			ge.argument = emptyArg;
			src->Notify(ge);
		}
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
			const char* evtStr = a_event.animEvent.c_str();
			if (!evtStr || !evtStr[0]) return RE::BSEventNotifyControl::kContinue;

			// Feed every animation event to the Animation Log for the UI
			if (AnimationLog::GetSingleton()->IsEnabled() && a_event.refr) {
				AnimationLog::GetSingleton()->AddAnimEvent(
					const_cast<RE::TESObjectREFR*>(a_event.refr), std::string(evtStr));
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		bool registered{ false };
	};

	static OARAnnotationSuppressionSink s_suppressionSink;

	void RegisterSuppressionSink()
	{
		if (s_suppressionSink.registered) return;
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

// Weapon change detection — called from Activate hook when we notice the weapon
// animation folder has changed. Proactively invalidates clones so they rebuild
// from fresh game animation data on next access.
static std::string s_lastKnownWeaponFolder;
static std::shared_mutex s_lastKnownWeaponMutex;

static void CheckAndInvalidateOnWeaponChange()
{
	std::string currentFolder;
	{
		std::shared_lock lock(s_graphAnimPathMutex);
		currentFolder = s_weaponAnimFolder;
	}

	std::unique_lock lock(s_lastKnownWeaponMutex);
	if (!currentFolder.empty() && currentFolder != s_lastKnownWeaponFolder) {
		logger::info("[OAR-WeaponChange] Weapon folder changed: '{}' -> '{}' — invalidating clones+state",
			s_lastKnownWeaponFolder, currentFolder);
		s_lastKnownWeaponFolder = currentFolder;
		lock.unlock();

		AnimationCache::GetSingleton()->InvalidateRuntimeClones();
		ClearClipRuntimeState();
	} else if (currentFolder.empty() && !s_lastKnownWeaponFolder.empty()) {
		// Weapon unequipped (holstered or no weapon) — also invalidate
		logger::info("[OAR-WeaponChange] Weapon folder cleared (was '{}') — invalidating clones+state",
			s_lastKnownWeaponFolder);
		s_lastKnownWeaponFolder.clear();
		lock.unlock();

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
			// Validate the original before writing it back.
			if (IsBadReadPtr(original, sizeof(uintptr_t))) continue;
			auto vtbl = *reinterpret_cast<uintptr_t*>(original);
			auto expected = cache->GetGameAnimVtable();
			if (expected != 0 && vtbl != expected) continue;
			// Single aligned pointer write — same operation as a normal swap.
			// Benign vs the render thread: both old and new pointers stay
			// valid (the clone buffer is retired, not freed, right after).
			*slot = original;
			restoredAnims++;
		}
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
	{
		std::unique_lock lock(s_originalAnimMutex);
		s_originalAnimMap.clear();
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

	static void BuildIdleAnimReverseMap()
	{
		if (s_idleAnimReverseBuilt.load()) return;

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

		auto* entries = reinterpret_cast<LoadedIdleAnimDataRaw*>(arrHeader->data);
		int captured = 0;
		int skipped = 0;
		{
			std::unique_lock lock(s_idleAnimReverseMutex);
			for (uint32_t i = 0; i < arrHeader->size; i++) {
				auto& e = entries[i];
				if (!e.clipGenerator) continue;

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
				if (!fileName || reinterpret_cast<uintptr_t>(fileName) < 0x10000 || !fileName[0]) continue;
				auto* clipGen = reinterpret_cast<RE::hkbClipGenerator*>(e.clipGenerator);
				s_idleAnimReverseMap[clipGen] = std::string(fileName);
				captured++;
			}
		}

		s_idleAnimReverseBuilt.store(true);
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

	// Given a raw suffix, check if it's directly registered. If so, return it.
	// If not, extract the leaf name and check the multi-leaf lookup table.
	// Returns "multi:<leaf>" if multiple candidates exist, the single candidate
	// if only one exists, or the original suffix if no leaf match is found.
	// Caller must hold NO locks on s_nameLookupMutex.
	static std::string ResolveOrLeafFallback(const std::string& a_suffix)
	{
		if (a_suffix.empty()) return a_suffix;

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

			if (!infoVec.empty()) {
				logger::info("[OAR] NameLookup: suffix='{}' -> '{}' ({} candidates)",
					suffix, infoVec[0]->replacementPath, infoVec.size());
			}
		}

		// Build leaf-to-full-suffix map for multi-match evaluation.
		// E.g., for suffixes "scar\wpnreload" and "wpnreload", both map to leaf "wpnreload".
		s_leafToFullSuffixes.clear();
		for (auto& [suffix, _] : s_suffixToInfos) {
			std::string leaf = suffix;
			auto lastSlash = suffix.rfind('\\');
			if (lastSlash != std::string::npos) {
				leaf = suffix.substr(lastSlash + 1);
			}
			s_leafToFullSuffixes[leaf].push_back(suffix);
		}

		// Sort: longer (more specific) paths first, then alphabetically
		for (auto& [leaf, suffixes] : s_leafToFullSuffixes) {
			std::ranges::sort(suffixes, [](const auto& a, const auto& b) {
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

		s_lookupBuilt = true;
		logger::info("[OAR] Built name lookup with {} suffix entries, {} leaf entries",
			s_suffixToReplacementPath.size(), s_leafToFullSuffixes.size());
	}

	static void PreloadReplacementAnimations()
	{
		auto* oar = OpenAnimationReplacer::GetSingleton();
		auto* cache = AnimationCache::GetSingleton();
		const auto& pathMap = oar->GetPathToSubModsMap();

		int loaded = 0;
		int failed = 0;

		for (auto& [mapKey, replacementInfos] : pathMap) {
			auto suffix = ExtractAnimSuffix(mapKey);
			if (suffix.empty()) continue;

			// Load EVERY SubMod's file for this original path. The cache keys
			// entries per (suffix, owning SubMod), so the Update hook can play
			// the condition-winning SubMod's actual file — previously only one
			// file per suffix was cached and a lower-priority mod's file could
			// play under a higher-priority mod's name (or vice versa).
			for (auto& info : replacementInfos) {
				if (info.absoluteDiskPath.empty()) {
					logger::warn("[OAR-Preload] No absolute path for suffix '{}'", suffix);
					failed++;
					continue;
				}

				if (cache->LoadAnimation(suffix, info.absoluteDiskPath, info.parentSubMod,
						info.parentSubMod ? info.parentSubMod->GetPriority() : 0)) {
					loaded++;
				} else {
					failed++;
				}
			}
		}

		logger::info("[OAR-Preload] Pre-loaded {} animations ({} failed), cache size: {}",
			loaded, failed, cache->GetCacheSize());
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
			using Ctor_t = void (*)(void*, const char*, bool, void*, bool);
			static REL::Relocation<Ctor_t> ctor{ REL::ID(1198116) };
			ctor(buffer, a_path, false, nullptr, false);
		}

		~SubgraphResourceProbe()
		{
			using Dtor_t = void (*)(void*);
			static REL::Relocation<Dtor_t> dtor{ REL::ID(1516202) };
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
	static void EnsureDirectSuffixForClip(RE::hkbClipGenerator* a_clip, const std::string& a_realPath)
	{
		if (!Settings::GetSingleton()->bDirectPathMatching || a_realPath.empty()) return;

		const auto exactSuffix = ExtractAnimSuffix(a_realPath);
		if (exactSuffix.empty()) return;

		{
			std::shared_lock lock(s_clipSuffixMutex);
			auto it = s_clipSuffixCache.find(a_clip);
			if (it != s_clipSuffixCache.end() && it->second == exactSuffix) return;
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
			s_clipSuffixCache[a_clip] = exactSuffix;
		}
		static std::atomic<int> s_rekeyLog{ 0 };
		if (s_rekeyLog.fetch_add(1, std::memory_order_relaxed) < 40) {
			logger::info("[OAR-DirectPath] Re-keyed clip {:X} suffix '{}' -> '{}' (real path '{}')",
				reinterpret_cast<uintptr_t>(a_clip), oldSuffix, exactSuffix, a_realPath);
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
		return ExtractAnimSuffix(cachedPath);
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
						return suffix;
					}
					// Not registered under the exact real-path suffix.
					// Direct path matching (default): the real path is the engine's
					// ground truth — if no replacement is registered under it, there
					// is NO replacement for this clip. Leaf matching stays available
					// only as a fallback for clips whose real path can't be resolved
					// (it never reaches here in that case — Source S failed).
					if (Settings::GetSingleton()->bDirectPathMatching) {
						return suffix;
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
			std::shared_lock smLock(s_activeSubModMutex);
			auto smIt = s_activeSubModMap.find(a_clipGen);
			if (smIt != s_activeSubModMap.end() && smIt->second &&
				!smIt->second->IsInterruptible() && !smIt->second->IsDisabled()) {
				for (auto* info : a_candidates) {
					if (info && info->parentSubMod == smIt->second) return info;
				}
			}
		}

		for (auto* info : a_candidates) {
			if (!info || !info->parentSubMod) continue;
			if (info->parentSubMod->IsDisabled()) continue;
			auto* cs = info->parentSubMod->GetConditionSet();
			if (!cs || cs->IsEmpty()) return info;
			if (!a_refr) continue;
			try {
				if (info->parentSubMod->EvaluateConditions(a_refr, a_clipGen)) return info;
			} catch (...) { continue; }
		}
		return nullptr;
	}

	void hkbClipGenerator_Activate(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context)
	{
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
		if (s_gameFullyLoaded.load() && s_hasActiveReplacements.load() && a_this && s_lookupBuilt) {
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
						preSwapAttempted = true;
						auto** animSlotPre = a_this->GetAnimationSlot();
						if (animSlotPre && *animSlotPre) {
							auto* cachePre = AnimationCache::GetSingleton();
							RE::hkaAnimation* replacement = nullptr;

							// Evaluate conditions NOW (same loop as the Update hook) so the
							// animation control is built from the winning SubMod's file rather
							// than a highest-priority guess that Update corrects later. When
							// no winner can be determined here (actor unresolvable, or all
							// conditions currently false), fall back to the highest-priority
							// file — pre-swapping SOMETHING keeps the control built from a
							// clone with nulled annotation tracks, which is the crash guard
							// this pre-swap exists for, and covers conditions that flip true
							// a few frames into the clip.
							RE::TESObjectREFR* refrPre = GetRefrFromContext(a_context);
							if (!refrPre) refrPre = RE::PlayerCharacter::GetSingleton();

							if (activeSuffix.size() > 6 && activeSuffix.substr(0, 6) == "multi:") {
								std::string leafName = activeSuffix.substr(6);
								std::shared_lock rlock(s_nameLookupMutex);
								auto leafIt = s_leafToFullSuffixes.find(leafName);
								if (leafIt != s_leafToFullSuffixes.end()) {
									// Pass 1: first suffix (most specific first — the vector is
									// pre-sorted) with a condition-passing winner.
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
									// Pass 2 (fallback): no winner anywhere — old behavior, first
									// suffix with any cached file.
									if (!replacement) {
										for (const auto& fullSuffix : leafIt->second) {
											replacement = cachePre->GetOrBuildRuntimeAnim(fullSuffix, *animSlotPre);
											if (replacement) break;
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
								// winner==nullptr falls back to the highest-priority file inside
								// the cache (owner=nullptr selects entries[0]).
								replacement = cachePre->GetOrBuildRuntimeAnim(activeSuffix, *animSlotPre,
									winner ? winner->parentSubMod : nullptr);
								if (replacement && winner) preSwapWinner = winner;
							}

							if (replacement) {
								auto repVtbl = *reinterpret_cast<uintptr_t*>(replacement);
								if (repVtbl >= 0x7FF000000000ull && repVtbl <= 0x7FFF00000000ull) {
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
		if (preSwapOriginal) {
			auto** animSlotPost = a_this->GetAnimationSlot();
			if (animSlotPost) {
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
		if (preSwapSucceeded && preSwapWinner && preSwapWinner->parentSubMod &&
			preSwapWinner->parentSubMod->GetReplaceAnnotations() &&
			!preSwapWinner->parentSubMod->GetPlayOnceFullBody()) {
			InstallReplacementTriggers(a_this, "");
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

		// Detect weapon folder changes and proactively invalidate stale clones
		CheckAndInvalidateOnWeaponChange();
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
				uintptr_t rawPtr = reinterpret_cast<uintptr_t>(a_this->animationName.stringAndFlag);
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

	// Deferred custom event queue — events are queued during hkbClipGenerator hooks
	// and fired AFTER RunActorUpdatesOrig() completes to avoid re-entrant graph traversal.
	struct DeferredEvent {
		RE::TESObjectREFR* refr;
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

	// Process all queued events (call from outside Havok update, e.g. HookedActorUpdate).
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

			// When ReloadEnd fires as a custom event, also force isReloading=false
			// so the state machine can transition (mirrors FPInertia Early ADS).
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
						auto realPath = ResolveClipPathFromSubgraph(a_this, a_context);
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
								suffix = exact;  // match against the REAL path from this frame on
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

		std::shared_lock lock(s_nameLookupMutex);

		if (isMultiMatch) {
			std::string leafName = suffix.substr(6);
			auto leafIt = s_leafToFullSuffixes.find(leafName);
			if (leafIt == s_leafToFullSuffixes.end() || leafIt->second.empty()) {
				return;
			}

			RE::TESObjectREFR* multiRefr = GetRefrFromContext(a_context);
			if (!multiRefr) multiRefr = RE::PlayerCharacter::GetSingleton();

			// Evaluate each candidate suffix's conditions, most-specific (longest) first
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
								auto expected = cache->GetGameAnimVtable();
								if (expected != 0 && vtbl == expected) {
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
			static int s_slotNullLog = 0;
			if (s_slotNullLog < 20) {
				logger::warn("[OAR-SlotNull] animSlot null for '{}' clipGen={:X} ctrl={:X}",
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
				// Current slot holds a game animation — validate before storing
				if (!IsBadReadPtr(current, sizeof(uintptr_t))) {
					auto vtbl = *reinterpret_cast<uintptr_t*>(current);
					auto expected = cache->GetGameAnimVtable();
					bool vtblOk = (expected != 0) ? (vtbl == expected)
						: (vtbl >= 0x7FF000000000ull && vtbl <= 0x7FFF00000000ull);
					if (vtblOk) {
						originalAnim = current;
						std::unique_lock olock(s_originalAnimMutex);
						auto [it, inserted] = s_originalAnimMap.try_emplace(a_this, originalAnim);
						if (!inserted) {
							originalAnim = it->second;
						}
					}
				}
			} else {
				// Slot has our replacement — recover the true original, but validate it
				RE::hkaAnimation* recovered = cache->GetOriginalFromReplacement(current);
				if (recovered && !IsBadReadPtr(recovered, sizeof(uintptr_t))) {
					auto vtbl = *reinterpret_cast<uintptr_t*>(recovered);
					auto expected = cache->GetGameAnimVtable();
					bool vtblOk = (expected != 0) ? (vtbl == expected)
						: (vtbl >= 0x7FF000000000ull && vtbl <= 0x7FFF00000000ull);
					if (vtblOk) {
						originalAnim = recovered;
						std::unique_lock olock(s_originalAnimMutex);
						auto [it, inserted] = s_originalAnimMap.try_emplace(a_this, recovered);
						if (!inserted) {
							originalAnim = it->second;
						}
					}
				}
				if (!originalAnim) {
					// Opportunistic re-arm: another clip (typically the other
					// perspective's graph, or this animation re-activating
					// elsewhere) may have rebuilt a clone for this suffix since
					// the invalidation, teaching the cache the FRESH game
					// original. Adopt it so condition changes work again for
					// this orphaned clip. Guards: vtable must be the game's,
					// and the track count must match the clone we're currently
					// playing (rejects an original from an incompatible
					// skeleton that merely shares the suffix).
					if (RE::hkaAnimation* fresh = cache->GetGameOriginalForSuffix(resolvedSuffix);
						fresh && fresh != current && !IsBadReadPtr(fresh, 0x20) &&
						!IsBadReadPtr(current, 0x20)) {
						auto vtbl = *reinterpret_cast<uintptr_t*>(fresh);
						auto expected = cache->GetGameAnimVtable();
						bool vtblOk = (expected != 0) ? (vtbl == expected)
							: (vtbl >= 0x7FF000000000ull && vtbl <= 0x7FFF00000000ull);
						// hkaAnimation: +0x18 = numTransformTracks
						auto freshTracks = *reinterpret_cast<int32_t*>(
							reinterpret_cast<uint8_t*>(fresh) + 0x18);
						auto currentTracks = *reinterpret_cast<int32_t*>(
							reinterpret_cast<uint8_t*>(current) + 0x18);
						if (vtblOk && freshTracks == currentTracks) {
							originalAnim = fresh;
							std::unique_lock olock(s_originalAnimMutex);
							auto [it, inserted] = s_originalAnimMap.try_emplace(a_this, fresh);
							if (!inserted) {
								originalAnim = it->second;
							}
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
				if (!originalAnim) {
					static int s_recoveryFailLog = 0;
					if (s_recoveryFailLog < 20) {
						logger::warn("[OAR-RecoveryFail] Can't recover valid original for '{}' clipGen={:X} current={:X}",
							resolvedSuffix, reinterpret_cast<uintptr_t>(a_this),
							reinterpret_cast<uintptr_t>(current));
						s_recoveryFailLog++;
					}
					return;
				}
			}
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
		if (hasPlayOnceCandidate) {
			std::shared_lock poLock(s_playOnceDecisionMutex);
			auto it = s_playOnceDecision.find(a_this);
			if (it != s_playOnceDecision.end()) {
				playOnceLocked = true;
				playOnceLockedResult = it->second;
			}
		}

		// "Interruptible" check: if the clip currently has an active replacement from
		// a non-interruptible submod, skip condition re-evaluation but still continue
		// to the replacement path for annotation firing and trigger maintenance.
		bool skipConditionEval = false;
		SubMod* lockedSubMod = nullptr;
		{
			std::shared_lock smLock(s_activeSubModMutex);
			auto smIt = s_activeSubModMap.find(a_this);
			if (smIt != s_activeSubModMap.end() && smIt->second) {
				if (!smIt->second->IsInterruptible() && !smIt->second->IsDisabled()) {
					bool allowReeval = false;

					// Check if a loop/echo event allows re-evaluation
					{
						std::unique_lock leLock(s_loopEchoFlagMutex);
						auto loopIt = s_clipLoopPending.find(a_this);
						if (loopIt != s_clipLoopPending.end() && loopIt->second) {
							if (smIt->second->GetReplaceOnLoop()) {
								allowReeval = true;
							}
							loopIt->second = false;
						}
						auto echoIt = s_clipEchoPending.find(a_this);
						if (echoIt != s_clipEchoPending.end() && echoIt->second) {
							if (smIt->second->GetReplaceOnEcho()) {
								allowReeval = true;
							}
							echoIt->second = false;
						}
					}

					if (!allowReeval) {
						skipConditionEval = true;
						lockedSubMod = smIt->second;
					}
				}
			}
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
				for (auto* info : candidates) {
					if (info && info->parentSubMod && info->parentSubMod->GetPlayOnceFullBody() &&
						!info->parentSubMod->IsDisabled()) {
						winningInfo = info;
						break;
					}
				}
			}
		} else {
			for (auto* info : candidates) {
				if (!info || !info->parentSubMod) continue;
				++totalCands;
				if (info->parentSubMod->IsDisabled()) { ++disabledCands; continue; }
				auto* cs = info->parentSubMod->GetConditionSet();
				if (!cs || cs->IsEmpty()) { shouldReplace = true; winningInfo = info; ++noCondCands; break; }
				if (!refr) continue;
				try {
					if (info->parentSubMod->EvaluateConditions(refr, a_this)) { shouldReplace = true; winningInfo = info; break; }
					++evalFalseCands;
				} catch (...) { continue; }
			}

			// Record the initial decision for playOnceFullBody candidates
			if (hasPlayOnceCandidate) {
				std::unique_lock poLock(s_playOnceDecisionMutex);
				s_playOnceDecision[a_this] = shouldReplace;
			}
		}

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
				logger::info("[OAR-Transition] '{}' shouldReplace {}->{} winner='{}' (cands total={} disabled={} evalFalse={} noCond={})",
					resolvedSuffix, prevKnown ? (prev ? "true" : "false") : "?",
					shouldReplace ? "true" : "false", winnerName,
					totalCands, disabledCands, evalFalseCands, noCondCands);

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
			auto* replacement = cache->GetOrBuildRuntimeAnim(cacheSuffix, originalAnim, winningOwner);
			bool bReplaceAnnot = winningInfo && winningInfo->parentSubMod ?
				winningInfo->parentSubMod->GetReplaceAnnotations() : true;

			// ---- Partial body (trackFilter) path ----
			// When the winning submod has trackFilter.enabled, do NOT swap the animation
			// slot. Instead register the replacement so Generate can sample it per-bone.
		bool useTrackFilter = winningInfo && winningInfo->parentSubMod &&
			winningInfo->parentSubMod->trackFilter.enabled && replacement;

			// Track-filtered clips ALWAYS fire replacement annotations. The original
			// animation stays in the slot and its triggers fire normally for base events.
			// The replacement's annotations (sounds, bone culls) must be fired manually
			// regardless of the replaceAnnotations setting, since the replacement anim
			// isn't in the slot and its triggers can't be read by the engine.
			if (useTrackFilter) bReplaceAnnot = true;
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
					state.replacement = replacement;
					state.parentSubMod = winningInfo->parentSubMod;
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
						float blendIn = winningInfo->parentSubMod->trackFilter.blendInTime;
						state.blendAlpha = (blendIn <= 0.0f) ? 1.0f : 0.0f;
						state.blendElapsed = 0.0f;
						state.blendDuration = blendIn;
						state.blendingOut = false;
					}
					// If re-registered while blending out, cancel blend-out
					if (state.blendingOut) {
						state.blendingOut = false;
						state.blendElapsed = 0.0f;
						state.blendDuration = winningInfo->parentSubMod->trackFilter.blendInTime;
					}
				}
			}
			if (*animSlot != originalAnim && originalAnim) {
				*animSlot = originalAnim;
			}
			// Record in activeSubModMap so the interruptible check works for track-filtered submods too
			if (winningInfo->parentSubMod) {
				std::unique_lock smLock(s_activeSubModMutex);
				s_activeSubModMap[a_this] = winningInfo->parentSubMod;
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
				if (repVtbl >= 0x7FF000000000ull && repVtbl <= 0x7FFF00000000ull) {
					if (*animSlot != replacement) {
						static int s_swapLog = 0;
						if (s_swapLog < 50) {
							logger::info("[OAR] Swapping clip '{}' -> replacement (conditions passed)", resolvedSuffix);
							s_swapLog++;
						}
						*animSlot = replacement;

						// When playOnceFullBody is active, keep original triggers intact so the
						// Havok state machine can still transition out of the current state
						// (e.g. reloadComplete → exit reload). Trigger NULLing blocks internal
						// hkbStateMachine transitions because it only reads from the trigger array.
						bool skipTriggerNull = (winningInfo && winningInfo->parentSubMod &&
							winningInfo->parentSubMod->GetPlayOnceFullBody());
						if (bReplaceAnnot && !skipTriggerNull) {
							InstallReplacementTriggers(a_this, cacheSuffix);
						}

						// Fire custom "on start" events and track active SubMod
						if (winningInfo && winningInfo->parentSubMod && refr) {
							QueueCustomEvents(refr, winningInfo->parentSubMod->eventsOnStart, "onStart");
							std::unique_lock smLock(s_activeSubModMutex);
							s_activeSubModMap[a_this] = winningInfo->parentSubMod;
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

					// Start full-body blend-in if SubMod has a blend time configured
					float blendTime = (winningInfo && winningInfo->parentSubMod)
						? winningInfo->parentSubMod->GetCustomBlendTimeOnInterrupt() : -1.0f;
					if (blendTime < 0.0f) blendTime = 0.0f;
					if (blendTime > 0.0f && originalAnim) {
						RE::TESObjectREFR* blendActor = refr ? refr : RE::PlayerCharacter::GetSingleton();
						if (blendActor) {
							ActorClipKey key{ blendActor, suffix };
							std::unique_lock fbLock(s_fullBodyBlendMutex);
							bool isNew = (s_fullBodyBlendMap.find(key) == s_fullBodyBlendMap.end());
							auto& bs = s_fullBodyBlendMap[key];
							if (isNew || bs.replacement != replacement) {
								bs.replacement = replacement;
								bs.original = originalAnim;
								bs.ownerClip = a_this;
								bs.blendElapsed = 0.0f;
								bs.blendAlpha = 0.0f;
								bs.blendingIn = true;
								bs.blendingOut = false;
								bs.blendDuration = blendTime;
								bs.poseSnapshotValid = false;
								bs.poseSnapshot.clear();
								if (isNew) s_fullBodyBlendActiveCount.fetch_add(1, std::memory_order_relaxed);
								static int s_fbRegLog = 0;
								if (s_fbRegLog < 10) {
									logger::info("[OAR-FullBodyBlend] Registered blend-in: suffix='{}' dur={:.2f}s",
										suffix, blendTime);
									s_fbRegLog++;
								}
							} else if (bs.blendingOut) {
								// Re-activation during blend-out: cancel, resume blend-in
								bs.blendingOut = false;
								bs.blendingIn = true;
								bs.blendElapsed = 0.0f;
								bs.poseSnapshotValid = false;
								bs.ownerClip = a_this;
							}
						}
					}
				if (bReplaceAnnot) {
					bool alreadyRestored = false;
					{
						std::lock_guard rg(s_triggersRestoredMutex);
						alreadyRestored = s_triggersRestoredSet.count(a_this) > 0;
					}
					if (!alreadyRestored) {
						EnsureReplacementTriggersInstalled(a_this, cacheSuffix);
					}
				} else {
					RestoreClipTriggers(a_this);
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
		if (bReplaceAnnot) {
			const auto& annotSuffix = cacheSuffix;
			// Same owner selection as the animation itself — the fired
			// annotations must come from the file that is actually playing.
			auto* annotations = AnimationCache::GetSingleton()->GetAnnotations(annotSuffix, winningOwner);
			if (annotations && !annotations->empty() && refr) {
				float localTime = a_this->GetLocalTime();
				std::vector<std::string> toFire;

				{
					std::unique_lock alock(s_annotStateMutex);
					auto& astate = s_annotStateMap[a_this];

					if (astate.activeSuffix != annotSuffix || astate.activeOwner != winningOwner) {
						astate.activeSuffix = annotSuffix;
						astate.activeOwner = winningOwner;
						astate.prevLocalTime = 0.f;
						astate.lastFiredIndex = -1;
						static int s_annotInitLog = 0;
						if (s_annotInitLog < 30) {
							logger::info("[OAR-Annot] Init tracking for '{}' ({} annotations, localTime={:.3f})",
								annotSuffix, annotations->size(), localTime);
							s_annotInitLog++;
						}
					}

					if (localTime >= 0.f) {
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
							for (int32_t i = astate.lastFiredIndex + 1; i < static_cast<int32_t>(annotations->size()); ++i) {
								auto& ann = (*annotations)[i];
								if (ann.time >= prevT) {
									toFire.push_back(ann.text);
								}
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

						for (int32_t i = astate.lastFiredIndex + 1; i < static_cast<int32_t>(annotations->size()); ++i) {
							auto& ann = (*annotations)[i];
							if (ann.time > curT) break;
							if (ann.time >= prevT) {
								toFire.push_back(ann.text);
								astate.lastFiredIndex = i;
							}
						}
					}
					astate.prevLocalTime = localTime;
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
					s_oarFiringAnnotations = true;
					for (auto& text : toFire) {
						static constexpr const char* kSoundPlayPrefix = "SoundPlay.";
						static constexpr size_t kSoundPlayLen = 10;

						if (text.size() > kSoundPlayLen &&
							_strnicmp(text.c_str(), kSoundPlayPrefix, kSoundPlayLen) == 0)
						{
							// SoundPlay annotations: play directly through BSAudioManager
							const char* soundName = text.c_str() + kSoundPlayLen;
							PlaySoundDirect(soundName, refr);
						} else {
							// All other annotations (CullBone, UncullBone, reloadComplete,
							// initiateStart, etc.): fire via BOTH behavior graph AND event
							// sinks. The graph handles bone visibility and state transitions;
							// the sinks handle audio plugins and other listeners.
							RE::BSFixedString evtName(text.c_str());
							refr->NotifyAnimationGraphImpl(evtName);
							NotifyEventSinks(refr, evtName);

							// When the replacement annotation fires "ReloadEnd", also
							// force isReloading=false and restore triggers so the state
							// machine can transition (mirrors FPInertia Early ADS).
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

						static int s_annotFireLog = 0;
						if (s_annotFireLog < 50) {
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
						logger::info("[OAR-Triggers] Restored triggers for '{}' (anim completed, localTime={:.3f} duration={:.3f}) [playOnce unlocked]",
							resolvedSuffix, localTime, duration);
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
						float blendOutTime = 0.0f;
						if (it->second.blendDuration > 0.0f) {
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
						}
					}
				}
			}

			if (!startedBlendOut) {
				if (!IsBadReadPtr(originalAnim, sizeof(uintptr_t))) {
					auto vtbl = *reinterpret_cast<uintptr_t*>(originalAnim);
					auto expected = cache->GetGameAnimVtable();
					bool ok = (expected != 0) ? (vtbl == expected)
						: (vtbl >= 0x7FF000000000ull && vtbl <= 0x7FFF00000000ull);
					if (ok) {
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
						}
						{
							std::lock_guard rg(s_triggersRestoredMutex);
							s_triggersRestoredSet.erase(a_this);
						}

						static int s_restoreLog = 0;
						if (s_restoreLog < 50) {
							logger::info("[OAR] Restoring original for clip '{}' (conditions failed/disabled)", suffix);
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

			// Do NOT restore the original animation or triggers during deactivation.
			// The clip is being freed and ALL backed-up pointers (animation, triggers)
			// will become stale. If the address is recycled by a new clip, stale entries
			// would cause crashes. Erase everything for this clip.
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

					if ((fbBlendingIn || fbBlendingOut) && fbAlpha > 0.001f && fbAlpha < 0.999f && fbOrigAnim && fbRepAnim) {
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

			// Non-source clips with valid cache and already-resolved bones:
			// handle entirely under shared_lock (no writes to shared state).
			// Condition re-evaluation in HookedActorUpdate handles cleanup when
			// conditions become false, so no staleness check is needed here.
			auto charIt = state.resolvedByChar.find(character);
			bool alreadyResolved = (charIt != state.resolvedByChar.end() &&
				charIt->second.version == filterPtr->version.load(std::memory_order_relaxed) &&
				!charIt->second.nameAndIndex.empty());

			if (!isSourceClip && state.cacheValid && alreadyResolved) {
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
		if (weight <= 0.001f) return;
		auto mode = filterPtr->mode;
		const float nowSec = s_tfNowSec.load(std::memory_order_relaxed);
		bool isSourceClip = (state.sourceClips.count(a_this) > 0);
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
			const bool haveBinding =
				trackToBoneArr && trackToBoneArr->data && trackToBoneArr->size > 0;

			static int s_pathLog = 0;
			if (s_pathLog < 3) {
				logger::info("[OAR-TrackFilter] Source path: clip={:X} haveBinding={} bindingTracks={}",
					reinterpret_cast<uintptr_t>(a_this), haveBinding,
					haveBinding ? trackToBoneArr->size : 0);
				s_pathLog++;
			}

			if (haveBinding) {
				// ============== Direct sampling path ==============
				const auto* trackToBoneData = reinterpret_cast<const int16_t*>(trackToBoneArr->data);
				const int32_t bindingNumTracks = trackToBoneArr->size;
				const int32_t animNumTracks = repAnim->numberOfTransformTracks;
				const int32_t numTracksToSample = std::min(animNumTracks, bindingNumTracks);
				if (numTracksToSample <= 0) return;

				float localTime = a_this->GetLocalTime();

				// The clip generator's localTime is driven by the ORIGINAL animation's
				// duration (since *animSlot isn't swapped for track filtering). Wrap it
				// to the replacement's duration so different-length replacements sample
				// correctly — preventing both out-of-bounds reads (replacement shorter)
				// and ensuring the full replacement plays through (replacement longer,
				// cycling independently of the base animation's loop).
				float repDuration = repAnim->duration;
				if (repDuration > 0.001f) {
					localTime = std::fmod(localTime, repDuration);
					if (localTime < 0.f) localTime += repDuration;
				}

				thread_local std::vector<RE::hkQsTransformRaw> tl_sampledTracks;
				thread_local std::vector<float> tl_sampledFloats;
				tl_sampledTracks.assign(numTracksToSample, RE::hkQsTransformRaw{});
				tl_sampledFloats.assign(std::max(1, repAnim->numberOfFloatTracks), 0.0f);

				repAnim->SamplePartialTracks(localTime,
					static_cast<uint32_t>(numTracksToSample),
					tl_sampledTracks.data(),
					static_cast<uint32_t>(repAnim->numberOfFloatTracks),
					tl_sampledFloats.data(),
					nullptr);

				static int s_bindingDiagLog = 0;
				bool wantBindingDiag = (s_bindingDiagLog < 3);
				if (wantBindingDiag) {
					logger::info("[OAR-TrackFilter-Binding] char={:X} bindingTracks={} animTracks={} localTime={:.3f}",
						reinterpret_cast<uintptr_t>(character),
						bindingNumTracks, animNumTracks, localTime);
				}

				for (auto& [name, idx] : cr.nameAndIndex) {
					if (idx < 0 || idx >= numOutputBones) continue;

					int32_t trackIdx = -1;
					for (int32_t t = 0; t < numTracksToSample; ++t) {
						if (trackToBoneData[t] == idx) { trackIdx = t; break; }
					}

					if (wantBindingDiag) {
						logger::info("[OAR-TrackFilter-Binding]   '{}': boneIdx={} trackIdx={} (identity={})",
							name, idx, trackIdx, (trackIdx == idx ? "yes" : "no"));
					}

					if (trackIdx < 0) continue;

					const RE::hkQsTransformRaw& repVal = tl_sampledTracks[trackIdx];
					RE::hkQsTransformRaw baseVal = outputPose[idx];

					state.cachedRepByName[name] = repVal;
					state.cachedBaseByName[name] = baseVal;

					if (mode == SubMod::TrackFilter::Mode::Override) {
						LerpTransform(outputPose[idx], repVal, weight);
					} else {
						BlendAdditiveTransform(outputPose[idx], baseVal, repVal, weight);
					}
					// Mark this bone as MODIFIED in the pose's bone mask, so the
					// engine's downstream pose composition honors our write.
					SetPoseBoneMaskBit(tracksPtr, poseHeader, idx);
				}
				if (wantBindingDiag) s_bindingDiagLog++;

				// onFraction is > 0 here (source path has an early-out at the top).

				state.cacheValid = true;
				state.lastSourceTimeSec = nowSec;

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
			if (!animSlot || !*animSlot) return;

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
				state2.cachedRepByName[name] = outputPose[idx];
				state2.cachedBaseByName[name] = tl_fullBasePose[idx];
			}
			state2.cacheValid = true;
			state2.lastSourceTimeSec = nowSec;

			memcpy(outputPose, tl_fullBasePose.data(), numOutputBones * sizeof(RE::hkQsTransformRaw));

			for (auto& [name, idx] : cr2.nameAndIndex) {
				if (idx < 0 || idx >= numOutputBones) continue;
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
			bool replacementActive = false;
			{
				std::shared_lock smLock(s_activeSubModMutex);
				auto smIt = s_activeSubModMap.find(a_this);
				replacementActive = (smIt != s_activeSubModMap.end() && smIt->second &&
					smIt->second->GetReplaceAnnotations());
			}
			if (replacementActive) {
				bool wasRestored = false;
				{
					std::lock_guard rg(s_triggersRestoredMutex);
					wasRestored = s_triggersRestoredSet.erase(a_this) > 0;
				}
				if (wasRestored) {
					InstallReplacementTriggers(a_this, "");
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
		Hooks::UpdateHooks::RunActorUpdatesOrig();

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
				// Track-filtered submods always re-evaluate conditions. Unlike full-body
				// replacements (where IsInterruptible prevents mid-animation interruption),
				// track filters overlay bones and must deactivate via blend-out when
				// conditions become false. The configured blendOutTime provides the smooth
				// transition — not the interruptible flag.
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

						if (condFalse && !state.blendingOut) {
							float deactivDelay = state.parentSubMod ? state.parentSubMod->GetDeactivationDelay() : 0.0f;
							if (deactivDelay > 0.0f && !state.deactivationDelayActive) {
								state.deactivationDelayActive = true;
								state.deactivationDelayRemaining = deactivDelay;
							}

							if (!state.deactivationDelayActive || state.deactivationDelayRemaining <= 0.0f) {
								state.deactivationDelayActive = false;
								state.blendingOut = true;
								state.blendElapsed = 0.0f;
								state.blendDuration = filterPtr ? filterPtr->blendOutTime : 0.0f;
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
						}

						if (state.deactivationDelayActive) {
							state.deactivationDelayRemaining -= dt;
						}

						if (state.blendingOut) {
							if (state.blendDuration <= 0.0f) {
								stIt = states.erase(stIt);
								s_trackFilterActiveCount.fetch_sub(1, std::memory_order_relaxed);
								continue;
							}
							state.blendElapsed += dt;
							float t = std::clamp(state.blendElapsed / state.blendDuration, 0.0f, 1.0f);
							state.blendAlpha = 1.0f - EaseInOutQuad(t);

							// Diagnostic: log blend-out progress every ~0.1s
							static float s_lastBlendLog = 0.0f;
							if (state.blendElapsed - s_lastBlendLog > 0.1f || state.blendAlpha <= 0.001f) {
								logger::info("[OAR-TrackFilter] BLEND-OUT tick: suffix='{}' elapsed={:.3f}/{:.3f} t={:.3f} alpha={:.4f} dt={:.4f}",
									state.suffix, state.blendElapsed, state.blendDuration, t, state.blendAlpha, dt);
								s_lastBlendLog = state.blendElapsed;
							}

							if (state.blendAlpha <= 0.001f) {
								logger::info("[OAR-TrackFilter] Blend-out COMPLETE — erasing entry for '{}'", state.suffix);
								s_lastBlendLog = 0.0f;
								stIt = states.erase(stIt);
								s_trackFilterActiveCount.fetch_sub(1, std::memory_order_relaxed);
								continue;
							}
						} else {
							float blendInTime = filterPtr ? filterPtr->blendInTime : 0.0f;
							if (blendInTime <= 0.0f || state.blendAlpha >= 1.0f) {
								state.blendAlpha = 1.0f;
							} else {
								state.blendElapsed += dt;
								float t = std::clamp(state.blendElapsed / blendInTime, 0.0f, 1.0f);
								state.blendAlpha = EaseInOutQuad(t);
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
						bs.blendAlpha = EaseInOutQuad(t);
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
						bs.blendAlpha = 1.0f - EaseInOutQuad(t);
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

namespace Hooks
{
	// === ActionFireEmpty hook via Actor vtable ===
	// IAnimationGraphManagerHolder::NotifyAnimationGraphImpl is vfunc index 1
	// on the IAnimationGraphManagerHolder vtable (Actor vtable entry index 5).
	namespace ActionFireEmptyHook
	{
		using NotifyFn = bool(*)(RE::IAnimationGraphManagerHolder*, const RE::BSFixedString&);
		static NotifyFn _OriginalNotify = nullptr;

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
			return _OriginalNotify(a_this, a_eventName);
		}

		void Install()
		{
			// Actor's IAnimationGraphManagerHolder vtable is at index 5 in the Actor vtable array
			REL::Relocation<uintptr_t> actorAnimGraphVtbl{ REL::ID(453840) };
			_OriginalNotify = reinterpret_cast<NotifyFn>(
				actorAnimGraphVtbl.write_vfunc(1, &HookedNotifyAnimGraph));
			logger::info("[OAR] ActionFireEmpty vtable hook installed (Actor::NotifyAnimationGraphImpl)");
		}
	}

	// Secondary detection: hook the PlayerControls "fire empty" code path directly.
	// REL::ID(818081) is the function called when the player presses fire with an empty
	// magazine. At offset 0x40A there's a call to DoAction that triggers the auto-reload.
	// By hooking this call site, we reliably detect fire-empty input at the PlayerControls
	// level — upstream of the animation graph, so it fires even if the anim event is
	// swallowed or doesn't reach NotifyAnimationGraphImpl.
	namespace PlayerFireEmptyHook
	{
		using DoActionFn = int64_t(*)(int64_t, int, unsigned int);
		static DoActionFn _OriginalDoAction = nullptr;

		static int64_t HookedDoAction(int64_t a_arg1, int a_arg2, unsigned int a_arg3)
		{
			// The player just pressed fire with an empty weapon — record timestamp.
			// We use formID 0x14 (player) since this only fires for the player character.
			{
				std::unique_lock lock{ s_fireEmptyMutex };
				auto& entry = s_fireEmptyMap[0x14];
				entry.timestamp = std::chrono::steady_clock::now();
				entry.generation++;
			}
			// Call through so the game's normal logic (or other mod hooks) still runs
			return _OriginalDoAction(a_arg1, a_arg2, a_arg3);
		}

		void Install(F4SE::Trampoline& trampoline)
		{
			REL::Relocation<std::uintptr_t> callLocation{ REL::ID(818081), 0x40A };
			_OriginalDoAction = reinterpret_cast<DoActionFn>(
				trampoline.write_call<5>(callLocation.address(), &HookedDoAction));
			logger::info("[OAR] PlayerFireEmpty hook installed (REL::ID 818081 + 0x40A)");
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
		PlayerFireEmptyHook::Install(F4SE::GetTrampoline());
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
			auto moduleBase = REL::Module::get().base();
			auto textSeg = REL::Module::get().segment(REL::Segment::Name::text);
			auto textEnd = textSeg.address() + textSeg.size();

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
			auto& trampoline = F4SE::GetTrampoline();
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

			oar->loadingPhase = "Loading animations...";
			PreloadReplacementAnimations();

			oar->isLoading.store(false);
			oar->loadingComplete.store(true);

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

			auto moduleBase = REL::Module::get().base();
			auto textSeg = REL::Module::get().segment(REL::Segment::Name::text);
			auto textEnd = textSeg.address() + textSeg.size();

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
			auto& trampoline = F4SE::GetTrampoline();
			RunActorUpdatesOrig = reinterpret_cast<RunActorUpdatesFn>(
				trampoline.write_call<5>(
					Offsets::ptr_RunActorUpdates.address() + Offsets::RunActorUpdates_Offset,
					reinterpret_cast<uintptr_t>(HookedActorUpdate)
				)
			);

			logger::info("[OAR] Actor update hook installed");
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
					s_fileRedirectMap[lowerKey] = info.replacementPath;
					logger::info("[OAR] FileRedirect: '{}' -> '{}'", lowerKey, info.replacementPath);
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

						// File redirect (existing behavior)
						if (s_fileMapBuilt) {
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
