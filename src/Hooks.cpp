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
// Stores the tick (steady_clock) when ActionFireEmpty was last dispatched per actor formID.
static std::shared_mutex s_fireEmptyMutex;
static std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> s_fireEmptyTimestamps;

// Window in which IsDryFiring reports true after ActionFireEmpty dispatch (ms).
// Kept short — just a brief pulse to trigger the submod. Users can pair with
// AnimationTime conditions to hold the replacement for longer if needed.
static constexpr int64_t kFireEmptyWindowMs = 150;

bool WasFireEmptyRecent(uint32_t a_formID)
{
	std::shared_lock lock{ s_fireEmptyMutex };
	auto it = s_fireEmptyTimestamps.find(a_formID);
	if (it == s_fireEmptyTimestamps.end()) return false;
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - it->second).count();
	return elapsed < kFireEmptyWindowMs;
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

	// Frame counter at the last source-clip Generate. Used to invalidate the cache
	// when source clips stop generating (without firing a Deactivate hook).
	uint64_t lastSourceFrame = 0;

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
static std::unordered_map<RE::TESObjectREFR*, CharTrackFilterState> s_charTrackFilterMap;
static std::atomic<int> s_trackFilterActiveCount{ 0 };

// Play Once (Full Body): tracks the initial replacement decision per clip generator.
// When a clip has a playOnceFullBody candidate, the first evaluation result is locked
// so that mid-animation condition flips in either direction are ignored.
static std::shared_mutex s_playOnceDecisionMutex;
static std::unordered_map<RE::hkbClipGenerator*, bool> s_playOnceDecision;

// Per-frame counter, incremented in HookedActorUpdate. Used for staleness detection.
static std::atomic<uint64_t> s_currentFrame{ 0 };
// Threshold: if no source clip has fired Generate for this many frames, the entry
// is considered stale and erased (so non-source clips stop applying old cached pose).
static constexpr uint64_t kTrackFilterStaleFrames = 300;

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

			for (auto& info : replacementInfos) {
				if (info.absoluteDiskPath.empty()) {
					logger::warn("[OAR-Preload] No absolute path for suffix '{}'", suffix);
					failed++;
					continue;
				}

				if (cache->LoadAnimation(suffix, info.absoluteDiskPath)) {
					loaded++;
				} else {
					failed++;
				}
				break;
			}
		}

		logger::info("[OAR-Preload] Pre-loaded {} animations ({} failed), cache size: {}",
			loaded, failed, cache->GetCacheSize());
	}

	static std::string GetClipSuffixFromContext(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context)
	{
		static int s_sourceLogCount = 0;
		static int s_diagLogCount = 0;

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

	void hkbClipGenerator_Activate(RE::hkbClipGenerator* a_this, const RE::hkbContext* a_context)
	{
		// PRE-SWAP: If we have a cached replacement for this clip, swap it in BEFORE
		// the original _Activate runs. This ensures the hkaDefaultAnimationControl
		// is built from our clone (which has NULLed annotationTracks), preventing
		// stale pointer crashes in computeMotion/clearAndDeallocate.
		RE::hkaAnimation* preSwapOriginal = nullptr;
		bool preSwapAttempted = false;
		bool preSwapSucceeded = false;
		if (s_gameFullyLoaded.load() && s_hasActiveReplacements.load() && a_this && s_lookupBuilt) {
			const char* clipName = a_this->animationName.data();
			if (clipName && reinterpret_cast<uintptr_t>(clipName) > 0x10000 && clipName[0] != '\0') {
				std::string activeSuffix = ResolveOrLeafFallback(ExtractAnimSuffix(std::string(clipName)));
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

							if (activeSuffix.size() > 6 && activeSuffix.substr(0, 6) == "multi:") {
								std::string leafName = activeSuffix.substr(6);
								std::shared_lock rlock(s_nameLookupMutex);
								auto leafIt = s_leafToFullSuffixes.find(leafName);
								if (leafIt != s_leafToFullSuffixes.end()) {
									for (const auto& fullSuffix : leafIt->second) {
										replacement = cachePre->GetOrBuildRuntimeAnim(fullSuffix, *animSlotPre);
										if (replacement) break;
									}
								}
							} else {
								replacement = cachePre->GetOrBuildRuntimeAnim(activeSuffix, *animSlotPre);
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
			// new original gets captured below.
			if (suffixChanged) {
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

		// Log this activation to the Animation Log for the UI
		if (AnimationLog::GetSingleton()->IsEnabled()) {
			RE::TESObjectREFR* refr = GetRefrFromContext(a_context);
			if (!refr) refr = RE::PlayerCharacter::GetSingleton();
			std::string suffixCopy;
			{
				std::shared_lock rlock(s_clipSuffixMutex);
				auto sit = s_clipSuffixCache.find(a_this);
				if (sit != s_clipSuffixCache.end()) suffixCopy = sit->second;
			}
			if (!suffixCopy.empty()) {
				AnimationLog::GetSingleton()->AddEntry(
					AnimationLog::EventType::kActivate,
					refr, suffixCopy, "", "");
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
			const char* clipName = a_this->animationName.data();
			if (clipName && reinterpret_cast<uintptr_t>(clipName) > 0x10000 && clipName[0] != '\0') {
				suffix = ResolveOrLeafFallback(ExtractAnimSuffix(std::string(clipName)));
				// Backfill the cache so Generate can find this clip's suffix later.
				// Clips that missed Activate (already active at hook install time) are
				// caught here.
				if (!suffix.empty()) {
					std::unique_lock lock(s_clipSuffixMutex);
					s_clipSuffixCache[a_this] = suffix;
				}
			}
		}

		if (suffix.empty()) return;

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
		// a non-interruptible submod, skip condition re-evaluation UNLESS a loop or
		// echo event occurred (matching Skyrim OAR behavior where non-interruptible
		// clips still re-evaluate at loop/echo boundaries).
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

					if (!allowReeval) return;
				}
			}
		}

		bool shouldReplace = false;
		ReplacementAnimFileInfo* winningInfo = nullptr;
		int totalCands = 0, disabledCands = 0, evalFalseCands = 0;
		int noCondCands = 0;

		if (playOnceLocked) {
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
			auto* replacement = cache->GetOrBuildRuntimeAnim(cacheSuffix, originalAnim);
			bool bReplaceAnnot = winningInfo && winningInfo->parentSubMod ?
				winningInfo->parentSubMod->GetReplaceAnnotations() : true;

			// ---- Partial body (trackFilter) path ----
			// When the winning submod has trackFilter.enabled, do NOT swap the animation
			// slot. Instead register the replacement so Generate can sample it per-bone.
		bool useTrackFilter = winningInfo && winningInfo->parentSubMod &&
			winningInfo->parentSubMod->trackFilter.enabled && replacement;
		if (useTrackFilter) {
			{
				std::unique_lock tfLock(s_trackFilterMutex);
				RE::TESObjectREFR* tfActor = refr;
				if (!tfActor) tfActor = RE::PlayerCharacter::GetSingleton();
				if (tfActor) {
					bool isNew = (s_charTrackFilterMap.find(tfActor) == s_charTrackFilterMap.end());
					auto& state = s_charTrackFilterMap[tfActor];
					// If the FILTER itself changed (different bone set), drop bone resolution cache.
					// Note: resolvedByChar only depends on skeleton + filter bone names, NOT on
					// which replacement animation is active. Don't clear it on replacement change.
					if (state.filter != &winningInfo->parentSubMod->trackFilter) {
						state.resolvedByChar.clear();
						state.cachedRepByName.clear();
						state.cachedBaseByName.clear();
						state.cacheValid = false;
					} else if (state.replacement != replacement) {
						// Replacement changed but filter is the same — only invalidate animation
						// sample caches, not bone resolution
						state.cachedRepByName.clear();
						state.cachedBaseByName.clear();
						state.cacheValid = false;
					}
					state.replacement = replacement;
					state.filter = &winningInfo->parentSubMod->trackFilter;
					state.parentSubMod = winningInfo->parentSubMod;
					state.sourceClip = a_this;
					state.sourceClips.insert(a_this);
					state.suffix = cacheSuffix;
					state.lastSourceFrame = s_currentFrame.load(std::memory_order_relaxed);
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
							AnimationLog::GetSingleton()->AddEntry(
								AnimationLog::EventType::kReplace,
								refr, resolvedSuffix, winningInfo->replacementPath, subModName);
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

		// Fire replacement annotations manually (only if replaceAnnotations is enabled).
		// When disabled, original triggers are kept so the game handles sounds/events natively.
		if (bReplaceAnnot) {
			const auto& annotSuffix = cacheSuffix;
			auto* annotations = AnimationCache::GetSingleton()->GetAnnotations(annotSuffix);
			if (annotations && !annotations->empty() && refr) {
				float localTime = a_this->GetLocalTime();
				std::vector<std::string> toFire;

				{
					std::unique_lock alock(s_annotStateMutex);
					auto& astate = s_annotStateMap[a_this];

					if (astate.activeSuffix != annotSuffix) {
						astate.activeSuffix = annotSuffix;
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
								AnimationLog::GetSingleton()->AddEntry(
									AnimationLog::EventType::kLoop,
									refr, suffix, annotSuffix, "");
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
				// Lock released — now fire annotations
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
						// Behavior events (weaponFire, reloadComplete, etc.):
						// fire via both behavior graph AND event sinks.
						RE::BSFixedString evtName(text.c_str());
						if (!SafeNotifyAnimGraph(refr, evtName)) {
							static int s_notifyFailLog = 0;
							if (s_notifyFailLog < 10) {
								logger::error("[OAR-SEH] NotifyAnimationGraphImpl crash caught for event '{}' — stale graph data",
									text);
								s_notifyFailLog++;
							}
						}
						SafeNotifyEventSinks(refr, evtName);

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
				if (duration > 0.01f && localTime >= duration - 0.01f) {
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

		// Conditions failed — if this clip's suffix matches the active track
		// filter's suffix, erase the ENTIRE entry. We can't rely on matching
		// a_this against sourceClips because the behavior graph frequently
		// creates new clip generator instances at different addresses for the
		// same animation (state transitions), leaving stale entries in the set.
		if (s_trackFilterActiveCount.load(std::memory_order_relaxed) > 0) {
			std::unique_lock tfLock(s_trackFilterMutex);
			RE::TESObjectREFR* tfActor = refr;
			if (!tfActor) tfActor = RE::PlayerCharacter::GetSingleton();
			if (tfActor) {
				auto it = s_charTrackFilterMap.find(tfActor);
				if (it != s_charTrackFilterMap.end() &&
					it->second.suffix == resolvedSuffix) {
					static int s_cleanupLog = 0;
					if (s_cleanupLog < 10) {
						logger::info("[OAR-TrackFilter] Conditions false for '{}' — erasing track filter entry for actor {:X}",
							resolvedSuffix, reinterpret_cast<uintptr_t>(tfActor));
						s_cleanupLog++;
					}
					s_charTrackFilterMap.erase(it);
					s_trackFilterActiveCount.fetch_sub(1, std::memory_order_relaxed);
				}
			}
		}

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
			if (s_trackFilterActiveCount.load(std::memory_order_relaxed) > 0) {
				std::unique_lock tfLock(s_trackFilterMutex);
				for (auto it = s_charTrackFilterMap.begin(); it != s_charTrackFilterMap.end(); ) {
					it->second.sourceClips.erase(a_this);
					if (it->second.sourceClip == a_this) it->second.sourceClip = nullptr;
					if (it->second.sourceClips.empty()) {
						it = s_charTrackFilterMap.erase(it);
						s_trackFilterActiveCount.fetch_sub(1, std::memory_order_relaxed);
					} else {
						++it;
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

		// --- Try shared_lock first for the fast non-source path ---
		// Non-source clips only READ cached data. We avoid unique_lock contention
		// that causes freezes when many clips fire during weapon events.
		{
			std::shared_lock tfShared(s_trackFilterMutex);
			auto it = s_charTrackFilterMap.find(actor);
			if (it == s_charTrackFilterMap.end()) return;

			auto& state = it->second;
			auto* filterPtr = state.filter;
			auto* replacement = state.replacement;
			if (!filterPtr || !filterPtr->enabled || !replacement) return;

			bool isSourceClip = (state.sourceClips.count(a_this) > 0);
			auto** animSlot = a_this->GetAnimationSlot();
			bool inRecursion = (animSlot && *animSlot == replacement);
			if (inRecursion) return;

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
					if (leafView.find("sneak") != std::string_view::npos) return;
				}

				const bool isAdditiveClip = (poseHeader.flags & 0x01) != 0;
				auto& cr = charIt->second;
				float weight = filterPtr->weight * state.blendAlpha;
				if (weight <= 0.001f) return;
				auto mode = filterPtr->mode;

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

		auto it = s_charTrackFilterMap.find(actor);
		if (it == s_charTrackFilterMap.end()) return;

		auto& state = it->second;
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
		uint64_t curFrame = s_currentFrame.load(std::memory_order_relaxed);
		bool isSourceClip = (state.sourceClips.count(a_this) > 0);
		auto** animSlot = a_this->GetAnimationSlot();
		bool inRecursion = (animSlot && *animSlot == replacement);

		static int s_genEntryLog = 0;
		if (s_genEntryLog < 6) {
			logger::info("[OAR-TrackFilter] Generate: char={:X} a_this={:X} isSource={} inRec={} bones={}",
				reinterpret_cast<uintptr_t>(character),
				reinterpret_cast<uintptr_t>(a_this),
				isSourceClip, inRecursion, cr.nameAndIndex.size());
			s_genEntryLog++;
		}

		// Don't re-enter our logic during the swap-fallback's recursive _Generate.
		if (inRecursion) return;

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
				state.lastSourceFrame = curFrame;

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

			it = s_charTrackFilterMap.find(actor);
			if (it == s_charTrackFilterMap.end()) {
				memcpy(outputPose, tl_fullBasePose.data(), numOutputBones * sizeof(RE::hkQsTransformRaw));
				return;
			}
			auto& state2 = it->second;

			for (auto& [name, idx] : cr.nameAndIndex) {
				if (idx < 0 || idx >= numOutputBones) continue;
				state2.cachedRepByName[name] = outputPose[idx];
				state2.cachedBaseByName[name] = tl_fullBasePose[idx];
			}
			state2.cacheValid = true;
			state2.lastSourceFrame = curFrame;

			memcpy(outputPose, tl_fullBasePose.data(), numOutputBones * sizeof(RE::hkQsTransformRaw));

			for (auto& [name, idx] : cr.nameAndIndex) {
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
				for (auto& [name, idx] : cr.nameAndIndex) {
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
				if (leafView.find("sneak") != std::string_view::npos) return;
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
	}

	void HookedActorUpdate()
	{
		s_currentFrame.fetch_add(1, std::memory_order_relaxed);

		// Process the original actor updates FIRST so game state is current.
		Hooks::UpdateHooks::RunActorUpdatesOrig();

		// Fire deferred custom events now that the Havok update cycle is complete.
		FlushDeferredEvents();

		// Compute frame delta time for blend ramping (shared by track filter + full-body)
		static auto s_lastTick = std::chrono::high_resolution_clock::now();
		auto now = std::chrono::high_resolution_clock::now();
		float dt = std::chrono::duration<float>(now - s_lastTick).count();
		dt = std::clamp(dt, 0.0001f, 0.1f);
		s_lastTick = now;

		// --- Track filter blend update ---
		if (s_trackFilterActiveCount.load(std::memory_order_relaxed) > 0) {
			struct PendingEval {
				RE::TESObjectREFR* actor;
				SubMod* subMod;
				std::string suffix;
				RE::hkbClipGenerator* sourceClip;
			};
			std::vector<PendingEval> toEval;
			{
				std::shared_lock tfShared(s_trackFilterMutex);
				toEval.reserve(s_charTrackFilterMap.size());
				for (auto& [actor, state] : s_charTrackFilterMap) {
					if (state.parentSubMod && actor)
						toEval.push_back({ actor, state.parentSubMod, state.suffix, state.sourceClip });
				}
			}

			std::unordered_set<RE::TESObjectREFR*> conditionsFalse;
			for (auto& pe : toEval) {
				if (pe.subMod->GetPlayOnceFullBody()) continue;
				// Track-filtered submods always re-evaluate conditions regardless of
				// the interruptible flag — they overlay bones and must deactivate
				// immediately when conditions become false (e.g., player crouches).
				// The interruptible flag is only meaningful for full-body replacements.
				if (!pe.subMod->EvaluateConditions(pe.actor, pe.sourceClip))
					conditionsFalse.insert(pe.actor);
			}

			{
				std::unique_lock tfLock(s_trackFilterMutex);
				for (auto it = s_charTrackFilterMap.begin(); it != s_charTrackFilterMap.end(); ) {
					auto& state = it->second;
					auto* filterPtr = state.filter;
					bool condFalse = conditionsFalse.count(it->first) > 0;

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
							it = s_charTrackFilterMap.erase(it);
							s_trackFilterActiveCount.fetch_sub(1, std::memory_order_relaxed);
							continue;
						}
						state.blendElapsed += dt;
						float t = std::clamp(state.blendElapsed / state.blendDuration, 0.0f, 1.0f);
						state.blendAlpha = 1.0f - EaseInOutQuad(t);
						if (state.blendAlpha <= 0.001f) {
							it = s_charTrackFilterMap.erase(it);
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
					++it;
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
					s_fireEmptyTimestamps[formID] = std::chrono::steady_clock::now();
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
				s_fireEmptyTimestamps[0x14] = std::chrono::steady_clock::now();
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
