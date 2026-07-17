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
		RE::hkaAnimation* animation{ nullptr };
		std::vector<uint8_t> fileData;
		std::string filePath;
		float duration{ 0.f };
		int32_t numTransformTracks{ 0 };
		int32_t numFloatTracks{ 0 };
		std::vector<uint32_t> vtableFixupOffsets;
		uint32_t sectionFileOffset{ 0 };
		std::unique_ptr<uint32_t[]> computedTransformOffsets;
		std::unique_ptr<uint32_t[]> computedFloatOffsets;

		std::vector<ParsedAnnotation> annotations;

		std::vector<uint8_t> runtimeStruct;
		RE::hkaAnimation* runtimeAnimation{ nullptr };
		RE::hkaAnimation* gameOriginal{ nullptr };

		// Per-file identity: the SubMod that provided this file (opaque tag,
		// only compared — never dereferenced) and its priority at load time.
		// Several SubMods can replace the same original path, so one suffix
		// maps to a LIST of cached files; lookups select by owner so the
		// condition-winning SubMod's actual file is the one that plays.
		const void* owner{ nullptr };
		int32_t priority{ 0 };
	};

	bool LoadAnimation(const std::string& a_suffix, const std::filesystem::path& a_absolutePath,
		const void* a_owner = nullptr, int32_t a_priority = 0);
	RE::hkaAnimation* GetCachedAnimation(const std::string& a_suffix) const;
	// a_owner: the winning SubMod (from condition evaluation). Selects that
	// SubMod's file under the suffix; falls back to the highest-priority file
	// when null or not found (pre-swap and legacy callers).
	RE::hkaAnimation* GetOrBuildRuntimeAnim(const std::string& a_suffix, RE::hkaAnimation* a_gameAnim,
		const void* a_owner = nullptr);
	const std::vector<ParsedAnnotation>* GetAnnotations(const std::string& a_suffix,
		const void* a_owner = nullptr) const;
	void SetVtableFromGame(uintptr_t a_vtable);
	uintptr_t GetGameAnimVtable() const { return m_gameAnimVtable.load(); }
	size_t GetCacheSize() const;
	bool IsOurReplacement(RE::hkaAnimation* a_anim) const;
	RE::hkaAnimation* GetOriginalFromReplacement(RE::hkaAnimation* a_replacement) const;
	// The freshest game original recorded for this suffix (set whenever any
	// clip rebuilds a clone). Used to opportunistically re-arm orphaned clips
	// whose own original was lost to an invalidation. May be null.
	RE::hkaAnimation* GetGameOriginalForSuffix(const std::string& a_suffix) const;
	void InvalidateRuntimeClones();
	void Clear();

private:
	AnimationCache() = default;

	bool ParsePackfile(CachedAnimation& a_entry);
	bool ParseTagfile(CachedAnimation& a_entry);
	RE::hkaAnimation* FindAnimationInBuffer(uint8_t* a_data, size_t a_size, uintptr_t a_vtable);
	static void ComputeSplineOffsets(uint8_t* a_animBytes, CachedAnimation& a_entry);

	// Caller must hold m_mutex (shared or unique).
	CachedAnimation* SelectEntry(const std::string& a_suffix, const void* a_owner) const;

	// Move a clone buffer into the keep-alive retirement list and reset the
	// entry's runtime fields. Caller must hold m_mutex (unique).
	//
	// WHY: active hkbClipGenerators hold a raw pointer into runtimeStruct.
	// Clearing/reusing that buffer while the game's render-job thread is still
	// generating from it zero-fills the clone's vtable in place -> the game
	// tail-calls through a null vtable (crash at decompressAnimation,
	// "jmp [rdx+0x28]" with rdx=0). A retired clone stays fully playable: its
	// vtable is the game's and its data pointers target fileData, which the
	// cache never frees — so a stale-referencing clip keeps playing it safely
	// until it deactivates.
	void RetireCloneLocked(CachedAnimation& a_entry);

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
	};
	std::vector<RetiredClone> m_retiredClones;
	// One suffix -> all files registered for it (one per SubMod replacing the
	// same original path; variants get their own suffixes but can still
	// collide across SubMods). Sorted by priority, highest first.
	std::unordered_map<std::string, std::vector<std::unique_ptr<CachedAnimation>>> m_cache;
	std::atomic<uintptr_t> m_gameAnimVtable{ 0 };
};
