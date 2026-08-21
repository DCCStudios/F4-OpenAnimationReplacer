#pragma once

#include <string>

class Settings
{
public:
	static Settings* GetSingleton()
	{
		static Settings singleton;
		return &singleton;
	}

	void Load();
	void Save();

	bool  bEnabled{ true };
	bool  bEnableUI{ true };
	bool  bAsyncParsing{ true };
	bool  bDisablePreloading{ false };
	bool  bFilterOutDuplicateAnimations{ true };
	bool  bShowWelcomeBanner{ true };

	// Direct path matching (default). When true, a clip whose REAL on-disk
	// animation path has been resolved (subgraph swap-array walk / per-frame
	// player poll) is matched against replacements by that exact path suffix
	// only — leaf-name matching is disabled for it, so a mod registered under
	// "scar\wpnreload" can no longer be applied to e.g. a 10mm reload just
	// because the leaf name matches. Leaf matching remains the FALLBACK for
	// clips whose real path could not be resolved (heuristic sources).
	// When false, the legacy leaf-matching behavior is used everywhere.
	bool  bDirectPathMatching{ true };

	// Optional cross-skeleton safety gate. When enabled, replacement candidates
	// whose authored skeleton root/perspective differs from the playing clip are
	// rejected. Disabled by default because special idles can legitimately play
	// a third-person-authored animation through the first-person behavior graph.
	bool  bSkeletonCompatibilityGate{ false };

	// Auto-reload mode. The engine's own automatic reloads (fire-on-empty and
	// last-round) are ALWAYS suppressed: they are attack-initiated, so the
	// engine exits the reload state the moment the reloadComplete annotation
	// refills the magazine, cutting the animation's tail short (same patches
	// as ManualReloadF4SE; safe alongside it). This mode selects what OAR does
	// instead — its replacement reloads go through the reload-key path
	// (PlayerControls::DoAction(kActionReload)), which carries no attack
	// context and therefore plays the full animation.
	//   0 = Auto-Reload On Last Round (default): reload when the magazine
	//       hits 0 by firing.
	//   1 = Auto-Reload On Fire Press When Empty: reload when the player
	//       presses fire with an empty magazine (vanilla-style trigger).
	//   2 = Suppress Auto-Reload: no automatic reloads at all; reload key only.
	int   iAutoReloadMode{ 0 };

	// Play a dry-fire click when the player presses fire on an empty magazine
	// (or with no ammo of that type at all). With the engine's fire-empty
	// auto-reload suppressed, the press would otherwise be silent. Uses the
	// equipped weapon's Attack Fail sound descriptor, falling back to the
	// vanilla WPNPistol10mmFireDry click when the weapon has none (same
	// behavior as the original ManualReloadF4SE mod).
	bool  bPlayDryFireSound{ true };

	// DIK scan code for the UI toggle hotkey. Default 0x3C = F2.
	std::uint32_t iToggleKey{ 0x3C };
	// When true, Shift must also be held with iToggleKey. Default off so F2 alone opens the UI.
	bool  bRequireShift{ false };
	bool  bPauseOnMenuOpen{ true };
	int   iEditorMode{ 0 };
	std::string sLanguage{ "en_US" };
	std::string sCJKFontPath{};
	// Text is intentionally larger than Dear ImGui's base size by default.
	// This multiplies the game window's Windows DPI scale. UIWindow applies the
	// combined ratio to window dimensions so increasing either scale does not
	// reduce the amount of editor content that remains visible.
	int   iTextSizePercent{ 125 };

	bool  bLogActivate{ true };
	bool  bLogReplace{ true };
	bool  bLogLoop{ true };
	bool  bLogEcho{ false };
	bool  bLogToFile{ false };
	int   iMaxLogEntries{ 100 };

	int   iAnimationLimit{ 16384 };
	int   iHavokHeapMultiplier{ 2 };

	uint64_t iLoadClipsAddressRVA{ 0 };

	bool  bEnableAnimationQueueProgressBar{ true };
	float fAnimationQueueLingerTime{ 5.0f };

	bool  bVerboseLogging{ false };

	static constexpr const char* kSettingsPath = "Data\\F4SE\\Plugins\\OpenAnimationReplacer.ini";

private:
	Settings() = default;
	Settings(const Settings&) = delete;
	Settings(Settings&&) = delete;
	~Settings() = default;
	Settings& operator=(const Settings&) = delete;
	Settings& operator=(Settings&&) = delete;
};
