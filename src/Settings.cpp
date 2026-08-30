#include "Settings.h"

#include <filesystem>

void Settings::Load()
{
	CSimpleIniA defaults;
	defaults.SetUnicode();

	const bool defaultsLoaded = (defaults.LoadFile(kSettingsPath) >= 0);
	if (!defaultsLoaded) {
		logger::warn("[OAR] No INI found at '{}' - using defaults", kSettingsPath);
	}

	// Single-INI model (user-directed): read and write ONE shipped INI
	// (kSettingsPath). The previous user.ini overlay is removed so the file the user
	// edits by hand and the file the Settings page writes are always the same one.
	auto getB = [&](const char* sec, const char* key, bool def) {
		return defaults.GetBoolValue(sec, key, def);
	};
	auto getI = [&](const char* sec, const char* key, int def) {
		return static_cast<int>(defaults.GetLongValue(sec, key, def));
	};
	auto getD = [&](const char* sec, const char* key, double def) {
		return defaults.GetDoubleValue(sec, key, def);
	};
	auto getS = [&](const char* sec, const char* key, const char* def) {
		return defaults.GetValue(sec, key, def);
	};

	bEnabled                      = getB("General", "bEnabled", bEnabled);
	bEnableUI                     = getB("General", "bEnableUI", bEnableUI);
	bAsyncParsing                 = getB("General", "bAsyncParsing", bAsyncParsing);
	bDisablePreloading            = getB("General", "bDisablePreloading", bDisablePreloading);
	bFilterOutDuplicateAnimations = getB("General", "bFilterOutDuplicateAnimations", bFilterOutDuplicateAnimations);
	bShowWelcomeBanner            = getB("General", "bShowWelcomeBanner", bShowWelcomeBanner);
	bDirectPathMatching           = getB("General", "bDirectPathMatching", bDirectPathMatching);
	bSkeletonCompatibilityGate    = getB("General", "bSkeletonCompatibilityGate", bSkeletonCompatibilityGate);
	bSeparateArchiveMods          = getB("General", "bSeparateArchiveMods", bSeparateArchiveMods);
	iAutoReloadMode               = std::clamp(getI("General", "iAutoReloadMode", iAutoReloadMode), 0, 2);
	bPlayDryFireSound             = getB("General", "bPlayDryFireSound", bPlayDryFireSound);

	iToggleKey   = static_cast<std::uint32_t>(defaults.GetLongValue("UI", "iToggleKey", static_cast<long>(iToggleKey)));
	bRequireShift = getB("UI", "bRequireShift", bRequireShift);
	bPauseOnMenuOpen = getB("UI", "bPauseOnMenuOpen", bPauseOnMenuOpen);
	iEditorMode = std::clamp(getI("UI", "iEditorMode", iEditorMode), 0, 2);
	iTextSizePercent = std::clamp(getI("UI", "iTextSizePercent", iTextSizePercent), 50, 200);
	sLanguage = getS("UI", "sLanguage", sLanguage.c_str());
	sCJKFontPath = getS("UI", "sCJKFontPath", sCJKFontPath.c_str());

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
	fAnimationQueueLingerTime = static_cast<float>(getD("UI", "fAnimationQueueLingerTime", fAnimationQueueLingerTime));

	bVerboseLogging = getB("Debug", "bVerboseLogging", bVerboseLogging);

	const char* rvaStr = getS("Debug", "iLoadClipsAddressRVA", "0");
	if (rvaStr) {
		iLoadClipsAddressRVA = std::strtoull(rvaStr, nullptr, 16);
	}

	logger::info("[OAR] Settings loaded: enabled={}, UI={}, async={}, animLimit={}, verbose={}, directPathMatching={}, skeletonGate={}, loadClipsRVA=0x{:X}",
		bEnabled, bEnableUI, bAsyncParsing, iAnimationLimit, bVerboseLogging,
		bDirectPathMatching, bSkeletonCompatibilityGate, iLoadClipsAddressRVA);
}

// Runtime verbose gate backing OAR_VLOG (declared in PCH.h). Deliberately does NOT
// touch the global spdlog level: the high-frequency diagnostics are gated per-call
// on this flag instead, so toggling verbose has no process-global side effect.
bool OAR_IsVerboseLogging()
{
	return Settings::GetSingleton() && Settings::GetSingleton()->bVerboseLogging;
}

void Settings::Save()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	// Single-INI model (user-directed): load, modify, and re-save the shipped INI in
	// place so hand edits and Settings-page writes share one file. Loading first
	// preserves comments/keys we don't manage.
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
	setB("General", "bDirectPathMatching", bDirectPathMatching);
	setB("General", "bSkeletonCompatibilityGate", bSkeletonCompatibilityGate);
	setB("General", "bSeparateArchiveMods", bSeparateArchiveMods);
	setI("General", "iAutoReloadMode", iAutoReloadMode);
	setB("General", "bPlayDryFireSound", bPlayDryFireSound);

	setI("UI", "iToggleKey", static_cast<int>(iToggleKey));
	setB("UI", "bRequireShift", bRequireShift);
	setB("UI", "bPauseOnMenuOpen", bPauseOnMenuOpen);
	setI("UI", "iEditorMode", iEditorMode);
	setI("UI", "iTextSizePercent", iTextSizePercent);
	ini.SetValue("UI", "sLanguage", sLanguage.c_str());
	ini.SetValue("UI", "sCJKFontPath", sCJKFontPath.c_str());
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

	const std::filesystem::path iniPath(kSettingsPath);
	std::error_code ec;
	std::filesystem::create_directories(iniPath.parent_path(), ec);
	if (ec) {
		logger::error("[OAR] Failed to create settings directory '{}': {}",
			iniPath.parent_path().string(), ec.message());
	}

	if (!ec && ini.SaveFile(kSettingsPath) >= 0) {
		logger::info("[OAR] Settings saved to '{}'", kSettingsPath);
	} else {
		logger::error("[OAR] Failed to save settings to '{}'", kSettingsPath);
	}
}
