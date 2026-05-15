#include "Settings.h"

void Settings::Load()
{
	CSimpleIniA ini;
	ini.SetUnicode();

	bool loaded = (ini.LoadFile(kSettingsPath) >= 0);
	if (!loaded) {
		logger::warn("[OAR] No INI found at '{}' - using defaults", kSettingsPath);
	}

	auto getB = [&](const char* sec, const char* key, bool def) {
		return ini.GetBoolValue(sec, key, def);
	};
	auto getI = [&](const char* sec, const char* key, int def) {
		return static_cast<int>(ini.GetLongValue(sec, key, def));
	};

	bEnabled                      = getB("General", "bEnabled", bEnabled);
	bEnableUI                     = getB("General", "bEnableUI", bEnableUI);
	bAsyncParsing                 = getB("General", "bAsyncParsing", bAsyncParsing);
	bDisablePreloading            = getB("General", "bDisablePreloading", bDisablePreloading);
	bFilterOutDuplicateAnimations = getB("General", "bFilterOutDuplicateAnimations", bFilterOutDuplicateAnimations);
	bShowWelcomeBanner            = getB("General", "bShowWelcomeBanner", bShowWelcomeBanner);

	iToggleKey   = static_cast<std::uint32_t>(ini.GetLongValue("UI", "iToggleKey", static_cast<long>(iToggleKey)));
	bRequireShift = getB("UI", "bRequireShift", bRequireShift);
	bPauseOnMenuOpen = getB("UI", "bPauseOnMenuOpen", bPauseOnMenuOpen);

	bLogActivate   = getB("AnimationLog", "bLogActivate", bLogActivate);
	bLogReplace    = getB("AnimationLog", "bLogReplace", bLogReplace);
	bLogLoop       = getB("AnimationLog", "bLogLoop", bLogLoop);
	bLogEcho       = getB("AnimationLog", "bLogEcho", bLogEcho);
	bLogToFile     = getB("AnimationLog", "bLogToFile", bLogToFile);
	iMaxLogEntries = getI("AnimationLog", "iMaxLogEntries", iMaxLogEntries);
	iMaxLogEntries = std::clamp(iMaxLogEntries, 10, 10000);

	iAnimationLimit      = getI("Limits", "iAnimationLimit", iAnimationLimit);
	iHavokHeapMultiplier = getI("Limits", "iHavokHeapMultiplier", iHavokHeapMultiplier);
	iAnimationLimit      = std::clamp(iAnimationLimit, 4096, 65535);
	iHavokHeapMultiplier = std::clamp(iHavokHeapMultiplier, 1, 8);

	bEnableAnimationQueueProgressBar = getB("UI", "bEnableAnimationQueueProgressBar", bEnableAnimationQueueProgressBar);
	fAnimationQueueLingerTime = static_cast<float>(ini.GetDoubleValue("UI", "fAnimationQueueLingerTime", fAnimationQueueLingerTime));

	bVerboseLogging = getB("Debug", "bVerboseLogging", bVerboseLogging);

	const char* rvaStr = ini.GetValue("Debug", "iLoadClipsAddressRVA", "0");
	if (rvaStr) {
		iLoadClipsAddressRVA = std::strtoull(rvaStr, nullptr, 16);
	}

	logger::info("[OAR] Settings loaded: enabled={}, UI={}, async={}, animLimit={}, verbose={}, loadClipsRVA=0x{:X}",
		bEnabled, bEnableUI, bAsyncParsing, iAnimationLimit, bVerboseLogging, iLoadClipsAddressRVA);
}

void Settings::Save()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(kSettingsPath);

	auto setB = [&](const char* sec, const char* key, bool val) {
		ini.SetBoolValue(sec, key, val);
	};
	auto setI = [&](const char* sec, const char* key, int val) {
		ini.SetLongValue(sec, key, static_cast<long>(val));
	};

	setB("General", "bEnabled", bEnabled);
	setB("General", "bEnableUI", bEnableUI);
	setB("General", "bAsyncParsing", bAsyncParsing);
	setB("General", "bDisablePreloading", bDisablePreloading);
	setB("General", "bFilterOutDuplicateAnimations", bFilterOutDuplicateAnimations);
	setB("General", "bShowWelcomeBanner", bShowWelcomeBanner);

	setI("UI", "iToggleKey", static_cast<int>(iToggleKey));
	setB("UI", "bRequireShift", bRequireShift);
	setB("UI", "bPauseOnMenuOpen", bPauseOnMenuOpen);
	setB("UI", "bEnableAnimationQueueProgressBar", bEnableAnimationQueueProgressBar);
	ini.SetDoubleValue("UI", "fAnimationQueueLingerTime", static_cast<double>(fAnimationQueueLingerTime));

	setB("AnimationLog", "bLogActivate", bLogActivate);
	setB("AnimationLog", "bLogReplace", bLogReplace);
	setB("AnimationLog", "bLogLoop", bLogLoop);
	setB("AnimationLog", "bLogEcho", bLogEcho);
	setB("AnimationLog", "bLogToFile", bLogToFile);
	setI("AnimationLog", "iMaxLogEntries", iMaxLogEntries);

	setI("Limits", "iAnimationLimit", iAnimationLimit);
	setI("Limits", "iHavokHeapMultiplier", iHavokHeapMultiplier);

	setB("Debug", "bVerboseLogging", bVerboseLogging);

	if (ini.SaveFile(kSettingsPath) >= 0) {
		logger::info("[OAR] Settings saved to '{}'", kSettingsPath);
	} else {
		logger::error("[OAR] Failed to save settings to '{}'", kSettingsPath);
	}
}
