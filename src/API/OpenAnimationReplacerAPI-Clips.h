#pragma once

// =============================================================================
// Open Animation Replacer F4 — Redistributable Clips API Header
// =============================================================================
// Plugin authors: copy this file into your project. It is self-contained
// (no OAR internals, no CommonLibF4 dependency).
//
// The Clips API lets other F4SE plugins query live data about the animation
// clips OAR sees on an actor: authored names, resolved on-disk paths, playback
// state (duration / position / speed / mode), 1st/3rd-person perspective,
// whether OAR replaced the clip (and with which submod, mod, priority and
// file), the annotations of the animation that is actually playing, the
// active-replacement list, and global statistics.
//
// THREADING: call every method from the GAME'S MAIN THREAD only (e.g. inside
// a task queued via F4SE's task interface, or your own main-thread hook).
// The clip queries walk live Havok graph structures.
//
// LIFETIME: ClipInfo::clipHandle identifies a clip generator only for the
// frame in which it was returned. Do not store handles across frames.
//
// Minimal example:
//
//   #include "OAR/OpenAnimationReplacerAPI-Clips.h"
//
//   void QueryPlayerClips() {   // main thread!
//       auto* api = OAR::Clips::GetAPI();
//       if (!api) return;       // OAR not installed / not loaded yet
//
//       OAR::Clips::ClipInfo clips[64];
//       uint32_t total = api->GetActorClips(0 /* player */, clips, 64);
//       for (uint32_t i = 0; i < std::min<uint32_t>(total, 64); ++i) {
//           // clips[i].resolvedPath, clips[i].duration, clips[i].localTime...
//       }
//   }
//
// =============================================================================

#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace OAR::Clips
{
	// API version — bump on breaking changes
	inline constexpr uint32_t kAPIVersion = 1;

	// ClipInfo::perspective values
	enum : uint8_t
	{
		kPerspectiveUnknown = 0,
		kPerspectiveFirstPerson = 1,
		kPerspectiveThirdPerson = 2,
	};

	// ClipInfo::playbackMode values (Havok hkbClipGenerator playback modes)
	enum : uint8_t
	{
		kModeSinglePlay = 0,
		kModeLooping = 1,
		kModeUserControlled = 2,
		kModePingPong = 3,
	};

	// ClipInfo::replacementKind values
	enum : uint8_t
	{
		kReplacementNone = 0,        // clip is playing its original animation
		kReplacementFullBody = 1,    // OAR swapped the animation in the clip's slot
		kReplacementTrackFilter = 2, // OAR overlays specific bones; slot holds the original
	};

	// =========================================================================
	// Data structs — fixed-size, C-ABI-safe. All strings are null-terminated
	// (truncated to the buffer size when longer).
	// =========================================================================

	struct ClipInfo
	{
		uint64_t clipHandle;       // opaque clip generator id; valid THIS FRAME only
		uint32_t actorFormID;
		uint8_t graphIndex;        // index into the actor's animation graph manager
		uint8_t perspective;       // kPerspective* (player 1st-person graph aware)
		uint8_t playbackMode;      // kMode*
		uint8_t replacementKind;   // kReplacement*
		float duration;            // seconds — the animation CURRENTLY playing
		                           // (the replacement's length when replaced)
		float localTime;           // current playback position in seconds
		float playbackSpeed;       // clip playback rate multiplier
		float originalDuration;    // the original animation's length; equals
		                           // duration when the clip is not replaced,
		                           // 0 when unknown
		int32_t subModPriority;    // priority of the active submod (0 when none)
		char animationName[128];   // authored clip animation path — may be a
		                           // template (e.g. "44pistol\wpnfire.hkx")
		                           // until the engine's real path resolves
		char resolvedPath[260];    // real on-disk path resolved from the
		                           // engine's subgraph data ("" until known)
		char suffix[128];          // OAR matching suffix: path after
		                           // "Animations\", no extension
		char subModName[128];      // active replacement's submod ("" when none)
		char modName[128];         // its parent replacer mod ("" when none)
		char replacementPath[260]; // replacement file path ("" when none)
	};

	struct ClipAnnotation
	{
		float time;     // seconds from clip start
		char text[124]; // annotation text (e.g. "WeaponFire", "SoundPlay.WPNRifleFire")
	};

	struct ActiveReplacementInfo
	{
		uint32_t actorFormID;
		bool conditionsPassed;
		char clipSuffix[128];
		char subModName[128];
		char replacementPath[260];
		char originalPath[260];    // resolved original path when known
		char actorName[64];
	};

	struct Stats
	{
		uint32_t replacerModCount;      // loaded replacer mods
		uint32_t subModCount;           // loaded submods across all mods
		uint32_t replacementAnimCount;  // registered replacement animations
		uint32_t cachedAnimFileCount;   // .hkx files loaded in the runtime cache
		uint32_t activeReplacementCount;// replacements applied right now
	};

	// =========================================================================
	// IClipsAPI — virtual interface returned by RequestPluginAPI_Clips
	// =========================================================================

	class IClipsAPI
	{
	public:
		virtual ~IClipsAPI() = default;

		// Returns the API version implemented by this OAR build.
		virtual uint32_t GetAPIVersion() const = 0;

		// Enumerate the active animation clips on an actor.
		// a_actorFormID: 0 or 0x14 = the player; otherwise any loaded actor.
		// Fills up to a_maxCount entries into a_outBuffer (may be null with
		// a_maxCount 0 to just count). Returns the TOTAL number of active
		// clips, which may exceed a_maxCount.
		virtual uint32_t GetActorClips(uint32_t a_actorFormID, ClipInfo* a_outBuffer, uint32_t a_maxCount) = 0;

		// Find one clip by name. a_nameOrLeaf is matched case-insensitively
		// against the leaf (file name, ".hkx" optional) of the clip's suffix,
		// authored animation name, and resolved path. Returns true and fills
		// a_out on the first match.
		virtual bool FindClip(uint32_t a_actorFormID, const char* a_nameOrLeaf, ClipInfo* a_out) = 0;

		// Annotations of the animation the clip is CURRENTLY playing — the
		// replacement's annotations when a replacement is installed, the
		// original's otherwise. a_clipHandle must come from GetActorClips /
		// FindClip in the same frame. Returns the total annotation count.
		virtual uint32_t GetClipAnnotations(uint64_t a_clipHandle, ClipAnnotation* a_outBuffer, uint32_t a_maxCount) = 0;

		// Snapshot of currently applied replacements.
		// a_actorFormID: 0 = all actors, otherwise filter to that actor.
		// Returns the total matching count (may exceed a_maxCount).
		virtual uint32_t GetActiveReplacements(uint32_t a_actorFormID, ActiveReplacementInfo* a_outBuffer, uint32_t a_maxCount) = 0;

		// Global counters. Returns false only when a_out is null.
		virtual bool GetStats(Stats* a_out) = 0;
	};

	// =========================================================================
	// GetAPI() — plugin authors call this to get the API interface
	// =========================================================================
	// Call at kPostLoad or later. Returns nullptr if OAR is not installed or
	// its DLL has not loaded yet. The returned pointer is valid for the whole
	// game session and may be cached.

	inline IClipsAPI* GetAPI()
	{
		HMODULE handle = GetModuleHandleA("OpenAnimationReplacer.dll");
		if (!handle) return nullptr;

		using RequestFn = IClipsAPI * (*)();
		auto fn = reinterpret_cast<RequestFn>(GetProcAddress(handle, "RequestPluginAPI_Clips"));
		return fn ? fn() : nullptr;
	}

} // namespace OAR::Clips
