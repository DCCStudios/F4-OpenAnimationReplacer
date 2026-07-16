#pragma once

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

	// DIK scan code for the UI toggle hotkey. Default 0x3C = F2.
	std::uint32_t iToggleKey{ 0x3C };
	// When true, Shift must also be held with iToggleKey. Default off so F2 alone opens the UI.
	bool  bRequireShift{ false };
	bool  bPauseOnMenuOpen{ true };
	int   iEditorMode{ 0 };

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
