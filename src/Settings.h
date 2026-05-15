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

	std::uint32_t iToggleKey{ 0x18 };
	bool  bRequireShift{ true };
	bool  bPauseOnMenuOpen{ true };

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
