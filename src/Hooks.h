#pragma once

#include "HavokTypes.h"

#include <string>
#include <vector>

void RegisterActorCharacter(RE::TESObjectREFR* a_refr);
void ClearCharacterCache();
void ClearClipRuntimeState();
// Save-load only: restore all hooked clips' originals/triggers while the
// recorded pointers are still valid. Call BEFORE ClearClipRuntimeState() and
// InvalidateRuntimeClones() at kPreLoadGame.
void RestoreAllActiveReplacements();
void SetGameFullyLoaded(bool a_loaded);
void SetHasActiveReplacements(bool a_has);
bool HasActiveReplacements();
void PopulateKnownStringData();
void RefreshWeaponAnimFolder();
void RegisterWeaponEquipListener();

// ActionFireEmpty detection — returns true if the engine dispatched ActionFireEmpty
// to the given actor within the last a_windowMs milliseconds.
bool WasFireEmptyRecent(uint32_t a_formID, int64_t a_windowMs);

// Returns a generation counter that increments each time ActionFireEmpty fires
// for the given actor. Used by the retriggerable logic to detect new presses.
uint32_t GetFireEmptyGeneration(uint32_t a_formID);

// ===== Clip query support (backs the external Clips API) ======================
// Internal, std::string-based representation of one active animation clip.
// The API layer (API/OpenAnimationReplacerAPI.cpp) copies these into the
// fixed-buffer POD structs defined by the redistributable SDK header.
struct OARClipQueryData
{
	uintptr_t clipHandle{ 0 };      // hkbClipGenerator address (opaque, valid this frame)
	uint32_t actorFormID{ 0 };
	uint8_t graphIndex{ 0 };        // index into the actor's BSAnimationGraphManager
	uint8_t perspective{ 0 };       // 0 = unknown, 1 = first person, 2 = third person
	uint8_t playbackMode{ 0 };      // RE::hkbClipGenerator_PlaybackMode
	uint8_t replacementKind{ 0 };   // 0 = none, 1 = full-body swap, 2 = track filter
	float duration{ 0.0f };         // seconds; animation CURRENTLY in the slot
	float localTime{ 0.0f };        // current playback position (seconds)
	float playbackSpeed{ 1.0f };
	float originalDuration{ 0.0f }; // original animation's duration (== duration when unreplaced)
	int32_t subModPriority{ 0 };
	std::string animationName;      // authored clip animation path (may be a template path)
	std::string resolvedPath;       // real on-disk path when the subgraph resolution succeeded
	std::string suffix;             // OAR matching suffix ("dir\leaf" after Animations\)
	std::string subModName;         // active replacement's SubMod (empty when none)
	std::string modName;            // active replacement's parent replacer mod (empty when none)
	std::string replacementPath;    // replacement file path (empty when none)
};

// Walks a_refr's animation graphs and fills a_out with every active clip
// generator's data. Returns the number of clips found. MAIN THREAD ONLY —
// walks live Havok graph structures the way the per-frame poll does.
size_t CollectActorClipQueryData(RE::TESObjectREFR* a_refr, std::vector<OARClipQueryData>& a_out);

// Fills a_out with (time, text) annotations of the animation currently playing
// on the given clip (replacement annotations when a replacement is installed).
// a_clipHandle must come from CollectActorClipQueryData in the same frame.
// Returns the number of annotations. MAIN THREAD ONLY.
size_t CollectClipAnnotations(uintptr_t a_clipHandle, std::vector<std::pair<float, std::string>>& a_out);

namespace Hooks
{
	void Install();

	namespace ClipGeneratorHooks
	{
		void Install();

		using ActivateFn = void(*)(RE::hkbClipGenerator*, const RE::hkbContext*);
		using UpdateFn = void(*)(RE::hkbClipGenerator*, const RE::hkbContext*, float);
		using DeactivateFn = void(*)(RE::hkbClipGenerator*, const RE::hkbContext*);
		using GenerateFn = void(*)(RE::hkbClipGenerator*, const RE::hkbContext*, const RE::hkbGeneratorOutput**, RE::hkbGeneratorOutput&, float);
		using StartEchoFn = void(*)(RE::hkbClipGenerator*, float);

		inline ActivateFn  _Activate{ nullptr };
		inline UpdateFn    _Update{ nullptr };
		inline DeactivateFn _Deactivate{ nullptr };
		inline GenerateFn  _Generate{ nullptr };
		inline StartEchoFn _StartEcho{ nullptr };
	}

	namespace LoadClipsHooks
	{
		void Install();
		bool TryDeferredInjection();

		using LoadClipsFn = void(*)(RE::hkbCharacterStringData*, void*, void*, RE::hkbBehaviorGraph*, const char*, void*);
		inline LoadClipsFn _LoadClips{ nullptr };
		inline LoadClipsFn _LoadClips2{ nullptr };
		inline bool bHookInstalled{ false };
	}

	namespace EnginePatchHooks
	{
		void Install();
	}

	namespace PreloadHooks
	{
		void Install();
		void PreloadReplacementAnimations(RE::BShkbAnimationGraph* a_graph);
	}

	namespace UpdateHooks
	{
		void Install();

		using RunActorUpdatesFn = void(*)();
		inline RunActorUpdatesFn RunActorUpdatesOrig{ nullptr };
	}

	namespace FileRedirectHooks
	{
		void Install();
		void BuildFileRedirectMap();
	}
}
