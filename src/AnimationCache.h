#pragma once

#include "HavokTypes.h"
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
		// Keep the replacement hkaAnimatedReferenceFrame when building the
		// runtime clone. Disabled by default because most weapon animation
		// replacements are pose-only and should not inherit donor root motion.
		bool preserveExtractedMotion{ false };

		// Disk identity at load time. A config reload recreates every SubMod,
		// so `owner` always misses — LoadAnimation matches by filePath instead
		// and RE-BINDS the entry to the new owner when size+mtime still match,
		// skipping the disk read/parse/clone rebuild entirely.
		uint64_t fileSize{ 0 };
		std::filesystem::file_time_type fileMTime{};
		// Set by MarkAllForRebind (config reload); cleared when LoadAnimation
		// re-binds or replaces the entry. Entries still flagged after the
		// re-parse belong to SubMods that no longer exist — PruneUnrebound
		// removes them so a deleted submod's file can never play again.
		bool pendingRebind{ false };
	};

	bool LoadAnimation(const std::string& a_suffix, const std::filesystem::path& a_absolutePath,
		const void* a_owner = nullptr, int32_t a_priority = 0,
		bool a_preserveExtractedMotion = false);
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
	void SetVtableFromGame(uintptr_t a_vtable);
	uintptr_t GetGameAnimVtable() const { return m_gameAnimVtable.load(); }
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
	RE::hkaAnimation* FindAnimationInBuffer(uint8_t* a_data, size_t a_size, uintptr_t a_vtable);
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
	// One suffix -> all files registered for it (one per SubMod replacing the
	// same original path; variants get their own suffixes but can still
	// collide across SubMods). Sorted by priority, highest first.
	std::unordered_map<std::string, std::vector<std::unique_ptr<CachedAnimation>>> m_cache;
	std::atomic<uintptr_t> m_gameAnimVtable{ 0 };
};
