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
// Global "Enabled" checkbox (Settings > General). Call whenever the value
// flips at runtime. Disabling queues a full vanilla restore that runs on the
// game thread on the next frame; enabling lets replacement resume on the next
// clip update. Safe to call from the UI (render) thread.
void OnGlobalEnabledChanged(bool a_enabled);
// "Reload All Configs": queues the full teardown + re-parse + lookup rebuild
// to run on the GAME thread, outside the Havok update cycle. NEVER run the
// reload directly on the UI (render) thread — the game thread can be mid
// graph-update holding pointers into the lookup tables the reload destroys
// (crash-2026-07-31-04-28-26: use-after-free in the clip Update hook).
void RequestConfigReload();
// Queues a re-sort of the per-suffix candidate vectors by current SubMod
// priority (game thread). Call after a priority edit so the change takes
// effect immediately instead of waiting for a full config reload.
void RequestLookupResort();
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

// Perspective of the animation graph that owns a playing clip. The player has
// simultaneous first- and third-person graphs, so this is deliberately clip
// specific rather than a camera-state query. Unknown means the player graph
// has not been identified yet and no resolved animation path is available.
enum class OARClipPerspective : uint8_t
{
	kUnknown = 0,
	kFirstPerson = 1,
	kThirdPerson = 2,
};
OARClipPerspective GetPlayingClipPerspective(RE::hkbClipGenerator* a_clip);

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

// Internal representation of one animation graph on an actor.
struct OARGraphQueryData
{
	uint32_t actorFormID{ 0 };
	uint8_t graphIndex{ 0 };
	bool isFirstPerson{ false };    // player-only knowledge (learned 1st-person graph)
	bool isRebuilding{ false };     // engine is rebuilding this graph's node list
	uint32_t activeNodeCount{ 0 };
	uint32_t activeClipCount{ 0 };  // clip generators among the active nodes
	uint32_t boneCount{ 0 };        // animation skeleton bone count
	uint32_t animationNameCount{ 0 }; // registered animation paths (character string data)
	uint32_t eventNameCount{ 0 };   // behavior event names (project string data)
	std::string characterName;      // Havok character name
	std::string projectAnimationPath; // project's animation root (often empty in FO4)
	std::string behaviorPath;       // project's behavior root
};

// One skeleton bone: name + parent index (-1 = root).
struct OARBoneQueryData
{
	int16_t index{ 0 };
	int16_t parentIndex{ -1 };
	std::string name;
};

// Graph-level collectors backing the Clips API v2 graph queries.
// All MAIN THREAD ONLY (same live-structure walk as the clip collector).
size_t CollectActorGraphQueryData(RE::TESObjectREFR* a_refr, std::vector<OARGraphQueryData>& a_out);
size_t CollectGraphBones(RE::TESObjectREFR* a_refr, uint32_t a_graphIndex, std::vector<OARBoneQueryData>& a_out);
size_t CollectGraphAnimationNames(RE::TESObjectREFR* a_refr, uint32_t a_graphIndex, std::vector<std::string>& a_out);
size_t CollectGraphEventNames(RE::TESObjectREFR* a_refr, uint32_t a_graphIndex, std::vector<std::string>& a_out);

// Fills a_out with (time, text) annotations of the animation currently playing
// on the given clip (replacement annotations when a replacement is installed).
// a_clipHandle must come from CollectActorClipQueryData in the same frame.
// Returns the number of annotations. MAIN THREAD ONLY.
size_t CollectClipAnnotations(uintptr_t a_clipHandle, std::vector<std::pair<float, std::string>>& a_out);

namespace Hooks
{
	void Install();

	namespace AutoReloadSuppression
	{
		// Per-frame driver for the auto-reload replication feature (triggers
		// OAR's own full-playback reload per the Auto-Reload mode setting while
		// the engine's truncating auto-reloads are suppressed). GAME THREAD
		// ONLY; called from the actor-update poll once the game is fully loaded.
		void PerFrameUpdate();
	}

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
