#pragma once

#include <string_view>

#include "HavokTypes.h"
#include <array>
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>

class AnimationCache
{
public:
	static AnimationCache* GetSingleton()
	{
		static AnimationCache singleton;
		return &singleton;
	}

	struct ParsedAnnotation
	{
		float time{ 0.f };
		std::string text;
	};

	struct CachedAnimation
	{
		// Which Havok class a serialized virtual-fixup slot belongs to. The
		// packfile's class-name section names it, so this is authoritative, not
		// guessed. Animation fixups receive the per-type game animation vtable;
		// a hkaDefaultAnimatedReferenceFrame fixup receives the reference-frame
		// vtable once that is resolved (any entry, so a stamped object is always
		// correctly typed), and until then the animation vtable exactly as every
		// earlier release did. The sibling hkaAnimatedReferenceFrame class is
		// never preserved and keeps the legacy animation-vtable stamp.
		enum class VtableFixupKind : uint8_t
		{
			kGameAnimation,
			kAnimatedReferenceFrame,
			kDefaultAnimatedReferenceFrame
		};
		struct VtableFixup
		{
			uint32_t offset{ 0 };
			VtableFixupKind kind{ VtableFixupKind::kGameAnimation };
		};

		RE::hkaAnimation* animation{ nullptr };
		std::vector<uint8_t> fileData;
		std::string filePath;
		float duration{ 0.f };
		int32_t numTransformTracks{ 0 };
		int32_t numFloatTracks{ 0 };
		std::vector<VtableFixup> vtableFixups;
		// hkaAnimation::m_type of the file's animation object (3 = spline
		// compressed). Selects WHICH game vtable the fixups receive: FO4 ships
		// several hkaAnimation subclasses (spline, lossless, interleaved, ...)
		// with different layouts, and stamping a spline donor with the lossless
		// vtable dispatches sampleTracks into code that reads the wrong fields
		// (crash-2026-09-01-01-58-59: idiv by a zero "numFrames" that was really
		// a spline field). Until the game has shown an animation of this type the
		// entry stays unpatched and every getter treats it as not loaded.
		int32_t animType{ 0 };
		bool vtablePatched{ false };
		uint32_t sectionFileOffset{ 0 };
		std::unique_ptr<uint32_t[]> computedTransformOffsets;
		std::unique_ptr<uint32_t[]> computedFloatOffsets;

		std::vector<ParsedAnnotation> annotations;

		// The donor file's OWN track->bone map, from its hkaAnimationBinding
		// (transformTrackToBoneIndices). Empty vector + bindingIdentity=false
		// means no binding was found (caller falls back to the host clip's
		// binding). bindingIdentity=true means the binding was found with an
		// empty index array — Havok's convention for identity (track i drives
		// bone i). Needed because a Leaf Matching donor is sampled under OTHER
		// weapons' clips, whose bindings describe THEIR animations' track
		// layout, not the donor's — indexing the donor's sampled tracks through
		// the host mapping stamps wrong data on wrong bones (MCX glitch,
		// 2026-08-16).
		std::vector<int16_t> trackToBoneIndices;
		bool bindingIdentity{ false };

		// One runtime clone per DISTINCT game original this file replaces.
		// A path-scoped file only ever sees one original (per weapon switch),
		// but a Leaf Matching file serves every clip sharing its filename —
		// 1st and 3rd person graphs at once, multiple weapons, NPCs — and a
		// single-clone entry would retire/rebuild every frame as the callers
		// alternate originals (each per-frame GetOrBuildRuntimeAnim passes its
		// own clip's original). Clone structs are ~0x110 bytes; keeping one per
		// original is cheap and each stays valid for the clips playing it.
		struct RuntimeClone
		{
			std::vector<uint8_t> structBuffer;
			RE::hkaAnimation* clone{ nullptr };
			RE::hkaAnimation* gameOriginal{ nullptr };
			// Identity of the original AT BUILD TIME. The engine can free an
			// original and reuse its address for a different animation; these
			// detect the swap so the stale clone is retired and rebuilt.
			float originalDuration{ 0.f };
			int32_t originalNumTracks{ 0 };
		};
		std::vector<RuntimeClone> clones;

		// Per-file identity: the SubMod that provided this file (opaque tag,
		// only compared — never dereferenced) and its priority at load time.
		// Several SubMods can replace the same original path, so one suffix
		// maps to a LIST of cached files; lookups select by owner so the
		// condition-winning SubMod's actual file is the one that plays.
		const void* owner{ nullptr };
		int32_t priority{ 0 };

		// Keep the replacement's extracted-motion reference frame when building
		// the runtime clone (opt-in via the SubMod flag; see ReplacerMods.h).
		bool preserveExtractedMotion{ false };
		// The extracted-motion pointer is preserved only when the packfile
		// identifies it as a supported hkaDefaultAnimatedReferenceFrame. Do not
		// infer this from a non-null +0x20 pointer: weapon packfiles commonly
		// carry other pointer-shaped data there.
		VtableFixupKind extractedMotionKind{ VtableFixupKind::kGameAnimation };
		bool hasExtractedMotionFixup{ false };

		// Source identity at load time. A config reload recreates every SubMod,
		// so `owner` always misses — the load functions match by filePath instead
		// and rebind the entry to the new owner when source size and identity
		// still match. Loose files also use their timestamp; BSResource sources
		// have no filesystem timestamp and use the Data-relative resource path.
		uint64_t fileSize{ 0 };
		std::filesystem::file_time_type fileMTime{};
		bool hasFileMTime{ false };
		// Set by MarkAllForRebind (config reload); cleared when LoadAnimation
		// re-binds or replaces the entry. Entries still flagged after the
		// re-parse belong to SubMods that no longer exist — PruneUnrebound
		// removes them so a deleted submod's file can never play again.
		bool pendingRebind{ false };
	};

	// Reverse identity for runtime clones. Replacement queries run from clip
	// update and lifecycle hooks; scanning every cached file/clone for each
	// query scales with the number of preloaded animations. Keep the lookup
	// value independent of CachedAnimation storage so it remains valid while a
	// clone is moved to the retired keep-alive list.
	struct CloneLookup
	{
		RE::hkaAnimation* gameOriginal{ nullptr };
		float originalDuration{ 0.f };
		int32_t originalNumTracks{ 0 };
		std::string suffix;
		const void* owner{ nullptr };
		bool retired{ false };
	};

	bool LoadAnimation(const std::string& a_suffix, const std::filesystem::path& a_absolutePath,
		const void* a_owner = nullptr, int32_t a_priority = 0,
		bool a_preserveExtractedMotion = false);
	bool LoadAnimationResource(const std::string& a_suffix, const std::string& a_resourcePath,
		const void* a_owner = nullptr, int32_t a_priority = 0,
		bool a_preserveExtractedMotion = false);
	// Read an archived (BSResource) text file fully into a_out via the same
	// resource-stream idiom LoadAnimationResource uses. Intended for small OAR
	// config.json / user.json files packaged inside a BA2. Returns false on any
	// failure (not found, empty, oversize, short read). Safe to call from the
	// background parser at kGameDataReady, the context LoadAnimationResource
	// targets. Static: uses no cache state.
	static bool ReadArchiveTextFile(const std::string& a_resourcePath, std::string& a_out);
	RE::hkaAnimation* GetCachedAnimation(const std::string& a_suffix,
		const void* a_owner = nullptr) const;
	// a_owner: the winning SubMod (from condition evaluation). Selects that
	// SubMod's file under the suffix; falls back to the highest-priority file
	// when null or not found (pre-swap and legacy callers).
	RE::hkaAnimation* GetOrBuildRuntimeAnim(const std::string& a_suffix, RE::hkaAnimation* a_gameAnim,
		const void* a_owner = nullptr);
	const std::vector<ParsedAnnotation>* GetAnnotations(const std::string& a_suffix,
		const void* a_owner = nullptr) const;
	// The donor's own track->bone map (see CachedAnimation::trackToBoneIndices).
	// Returns true when the file's binding was found; a_outMap empty with
	// a_outIdentity=true means identity mapping.
	bool GetDonorTrackMap(const std::string& a_suffix, const void* a_owner,
		std::vector<int16_t>& a_outMap, bool& a_outIdentity) const;
	// Learn the game vtable for a_gameAnim's animation type (see
	// CachedAnimation::animType) and patch every deferred cached entry of that
	// type. Cheap when the type is already known; call from the clip hooks with
	// the clip's live animation. Returns true when a new type was captured.
	bool CaptureGameVtable(const RE::hkaAnimation* a_gameAnim);
	uintptr_t GetGameAnimVtable(int32_t a_type) const
	{
		return (a_type > 0 && a_type < kMaxAnimTypes) ? m_gameAnimVtableByType[a_type].load() : 0;
	}
	// The spline-compressed vtable: every donor file OAR has ever seen is type 3,
	// so this is the "can replacements play yet" gate legacy callers ask for.
	uintptr_t GetGameAnimVtable() const { return GetGameAnimVtable(kAnimType_SplineCompressed); }
	// True when a_vtbl is one of the vtables captured from live game animations.
	// Anything OAR hands to the engine or samples itself must pass this.
	bool IsKnownGameVtable(uintptr_t a_vtbl) const;
	size_t GetCacheSize() const;
	bool IsOurReplacement(RE::hkaAnimation* a_anim) const;
	RE::hkaAnimation* GetOriginalFromReplacement(RE::hkaAnimation* a_replacement) const;
	// Return the exact game animation replaced by a RETIRED clone. Unlike
	// GetOriginalFromReplacement, this deliberately ignores live clones so
	// lifecycle hooks can scrub invalidated shared bindings without disrupting
	// a legitimate replacement that is still current for the equipped weapon.
	RE::hkaAnimation* GetOriginalFromRetiredReplacement(RE::hkaAnimation* a_replacement) const;
	// Reverse-lookup the suffix + owning SubMod for a clone still installed in a
	// clip's animation slot. Checks live cache entries first, then RETIRED
	// clones (kept-alive buffers whose owning clip survived an invalidation,
	// e.g. a mid-session save load). Returns false when the pointer is not one
	// of ours. outOwner is an opaque tag (a SubMod*): compare it against the
	// live registry before using — a config reload can retire clones whose
	// owner pointer no longer refers to a live SubMod.
	bool GetReplacementIdentity(RE::hkaAnimation* a_anim, std::string& a_outSuffix,
		const void*& a_outOwner) const;
	// The freshest game original recorded for this suffix (set whenever any
	// clip rebuilds a clone). Used to opportunistically re-arm orphaned clips
	// whose own original was lost to an invalidation. May be null.
	RE::hkaAnimation* GetGameOriginalForSuffix(const std::string& a_suffix) const;
	void InvalidateRuntimeClones();
	void Clear();

	// Config-reload support (see CachedAnimation::pendingRebind):
	// MarkAllForRebind flags every entry BEFORE the re-parse; the preload then
	// re-binds surviving files in place; PruneUnrebound removes whatever was
	// not re-bound (its submod was deleted/renamed), retiring clone AND file
	// buffers because live clips may still reference them. Returns the number
	// of entries pruned.
	void MarkAllForRebind();
	size_t PruneUnrebound();

private:
	AnimationCache() = default;

	bool ParsePackfile(CachedAnimation& a_entry);
	bool ParseTagfile(CachedAnimation& a_entry);
	// Bodies of the two BA2-resource readers, called from their SEH-wrapped public
	// counterparts. Split out so the __try wrapper holds no C++ objects that require
	// unwinding (the game's BSResourceNiBinaryStream ctor can raise an access
	// violation during startup archive registration; an AV is SEH, not C++, so a
	// try/catch cannot catch it — the wrapper must use __try/__except).
	bool DoLoadAnimationResourceUnsafe(const std::string& a_suffix, const std::string& a_resourcePath,
		const void* a_owner, int32_t a_priority, bool a_preserveExtractedMotion);
	static bool DoReadArchiveTextFileUnsafe(const std::string& a_resourcePath, std::string& a_out);
	bool TryRebindCached(const std::string& a_suffix, std::string_view a_sourceIdentity,
		bool a_hasFileMTime, std::uint64_t a_sourceSize,
		std::filesystem::file_time_type a_sourceMTime, const void* a_owner,
		int32_t a_priority, bool a_preserveExtractedMotion);
	bool LoadAnimationBytes(const std::string& a_suffix, std::string a_sourceIdentity,
		std::vector<uint8_t>&& a_bytes, bool a_hasFileMTime, uint64_t a_sourceSize,
		std::filesystem::file_time_type a_sourceMTime, const void* a_owner, int32_t a_priority,
		bool a_preserveExtractedMotion);
	RE::hkaAnimation* FindAnimationInBuffer(uint8_t* a_data, size_t a_size, uintptr_t a_vtable);
	// Resolve the hkaDefaultAnimatedReferenceFrame vtable from CommonLib's
	// Address Library (id 587967 — verified stable across OG/NG/AE), and, on
	// first resolution, stamp already-parsed reference-frame fixups and retire
	// preserve-enabled clones built before it was known. Resolved lazily, only
	// for opt-in entries, so pose-only setups never depend on the id. Returns 0
	// only when the resolved address fails post-resolution validation.
	uintptr_t EnsureReferenceFrameVtable();
	static void ComputeSplineOffsets(uint8_t* a_animBytes, CachedAnimation& a_entry);

	// Caller must hold m_mutex (shared or unique).
	CachedAnimation* SelectEntry(const std::string& a_suffix, const void* a_owner) const;

	// Move ONE clone's buffer into the keep-alive retirement list and remove
	// it from the entry's clone set. Caller must hold m_mutex (unique).
	void RetireSingleCloneLocked(CachedAnimation& a_entry, size_t a_index,
		const std::string& a_suffix);

	// Retire EVERY clone of the entry into the keep-alive retirement list.
	// Caller must hold m_mutex (unique).
	// a_retireBackingData: also move the entry's fileData into the retirement
	// record. REQUIRED whenever the entry itself is about to be destroyed or
	// replaced — the clone's spline data pointers target fileData, and clones
	// retired on EARLIER invalidations still point into it too.
	//
	// WHY: active hkbClipGenerators hold a raw pointer into a clone's
	// structBuffer.
	// Clearing/reusing that buffer while the game's render-job thread is still
	// generating from it zero-fills the clone's vtable in place -> the game
	// tail-calls through a null vtable (crash at decompressAnimation,
	// "jmp [rdx+0x28]" with rdx=0). A retired clone stays fully playable: its
	// vtable is the game's and its data pointers target fileData, which the
	// cache never frees — so a stale-referencing clip keeps playing it safely
	// until it deactivates.
	void RetireCloneLocked(CachedAnimation& a_entry, bool a_retireBackingData = false,
		const std::string& a_suffix = {});

	mutable std::shared_mutex m_mutex;
	// Retired clone buffers, kept alive because clips may still reference them
	// after invalidation (weapon switch, save load). Capped; oldest dropped
	// first — by the time hundreds of invalidations have passed, no clip from
	// the oldest one can still be active.
	//
	// clonePtr is remembered so IsOurReplacement() still recognizes a retired
	// clone sitting in a clip's animation slot. Without it, the Update hook
	// misclassifies the stale clone as a GAME animation (its vtable is the
	// game's, memcpy'd at build time), adopts it as the clip's "original", and
	// feeds it back into the clone builder as the copy source — the exact
	// self-memcpy that zeroed a clone's vtable and crashed in
	// decompressAnimation (crash-2026-07-17-00-10-31: base == gameStruct).
	struct RetiredClone
	{
		std::vector<uint8_t> buffer;
		RE::hkaAnimation* clonePtr{ nullptr };
		// Preserve the exact game animation this clone replaced. A weapon switch
		// can retire the clone while a shared binding still points at it; keeping
		// this reverse link lets the activation scrub restore that binding instead
		// of treating the retired clone as an unrecoverable game animation.
		RE::hkaAnimation* gameOriginal{ nullptr };
		float originalDuration{ 0.f };
		int32_t originalNumTracks{ 0 };
		// Backing .hkx file buffer of a destroyed/replaced entry (see
		// RetireCloneLocked's a_retireBackingData). Empty for plain clone
		// invalidations, where fileData stays in the live entry.
		std::vector<uint8_t> backingFileData;
		// Identity carried from the entry this clone came from, so a clip still
		// holding the retired clone in its animation slot after a save-load
		// invalidation can be re-identified (suffix it plays + owning SubMod).
		// owner is an opaque tag (a SubMod*), only ever compared against the
		// LIVE registry before use — never dereferenced blind. See
		// GetReplacementIdentity.
		std::string suffix;
		const void* owner{ nullptr };
	};
	std::vector<RetiredClone> m_retiredClones;
	// One entry per live or retired runtime clone. Protected by m_mutex.
	std::unordered_map<RE::hkaAnimation*, CloneLookup> m_cloneLookup;
	// One suffix -> all files registered for it (one per SubMod replacing the
	// same original path; variants get their own suffixes but can still
	// collide across SubMods). Sorted by priority, highest first.
	std::unordered_map<std::string, std::vector<std::unique_ptr<CachedAnimation>>> m_cache;

	// Game vtables indexed by hkaAnimation::m_type. Havok 2014's AnimationType
	// enum is small (interleaved=1, mirrored=2, spline=3, quantized=4,
	// predictive=5, reference pose=6, lossless above that); 16 leaves headroom.
	static constexpr int32_t kMaxAnimTypes = 16;
	static constexpr int32_t kAnimType_SplineCompressed = 3;
	std::array<std::atomic<uintptr_t>, kMaxAnimTypes> m_gameAnimVtableByType{};

	// hkaDefaultAnimatedReferenceFrame vtable (extracted-motion preservation).
	// Resolved lazily by EnsureReferenceFrameVtable; 0 until an opt-in entry
	// needs it. Separate from the animation vtables: reference-frame fixups are
	// a different Havok class and must never receive an animation vtable.
	std::atomic<uintptr_t> m_referenceFrameVtable{ 0 };
	// Set once any loaded entry opts into extracted motion. Lets the per-call
	// hot path in GetOrBuildRuntimeAnim skip the lock-and-peek entirely when
	// the feature is unused (one relaxed load instead of a shared_lock + lookup).
	std::atomic<bool> m_anyPreserveEntries{ false };

	// Stamp the entry's virtual fixups (and the animation object itself) with
	// the game vtable for its animType, if known. Idempotent. Returns the number
	// of slots written (0 when the type's vtable is still unknown). Only touches
	// the entry's own buffer, so it is safe before the entry is published; for
	// published entries the caller holds m_mutex (unique).
	int ApplyEntryVtables(CachedAnimation& a_entry) const;
};
