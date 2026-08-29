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

	CSimpleIniA user;
	user.SetUnicode();
	const bool userLoaded = (user.LoadFile(kUserSettingsPath) >= 0);
	if (userLoaded) {
		logger::info("[OAR] User settings loaded from '{}' (defaults: '{}')",
			kUserSettingsPath, kSettingsPath);
	}

	auto getB = [&](const char* sec, const char* key, bool def) {
		return user.GetBoolValue(sec, key, defaults.GetBoolValue(sec, key, def));
	};
	auto getI = [&](const char* sec, const char* key, int def) {
		return static_cast<int>(user.GetLongValue(sec, key, defaults.GetLongValue(sec, key, def)));
	};
	auto getD = [&](const char* sec, const char* key, double def) {
		return user.GetDoubleValue(sec, key, defaults.GetDoubleValue(sec, key, def));
	};
	auto getS = [&](const char* sec, const char* key, const char* def) {
		return user.GetValue(sec, key, defaults.GetValue(sec, key, def));
	};

	bEnabled                      = getB("General", "bEnabled", bEnabled);
	bEnableUI                     = getB("General", "bEnableUI", bEnableUI);
	bAsyncParsing                 = getB("General", "bAsyncParsing", bAsyncParsing);
	bDisablePreloading            = getB("General", "bDisablePreloading", bDisablePreloading);
	bFilterOutDuplicateAnimations = getB("General", "bFilterOutDuplicateAnimations", bFilterOutDuplicateAnimations);
	bShowWelcomeBanner            = getB("General", "bShowWelcomeBanner", bShowWelcomeBanner);
	bDirectPathMatching           = getB("General", "bDirectPathMatching", bDirectPathMatching);
	bSkeletonCompatibilityGate    = getB("General", "bSkeletonCompatibilityGate", bSkeletonCompatibilityGate);
	iAutoReloadMode               = std::clamp(getI("General", "iAutoReloadMode", iAutoReloadMode), 0, 2);
	bPlayDryFireSound             = getB("General", "bPlayDryFireSound", bPlayDryFireSound);

	iToggleKey   = static_cast<std::uint32_t>(user.GetLongValue("UI", "iToggleKey",
		defaults.GetLongValue("UI", "iToggleKey", static_cast<long>(iToggleKey))));
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

void Settings::Save()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(kUserSettingsPath);

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

	const std::filesystem::path userPath(kUserSettingsPath);
	std::error_code ec;
	std::filesystem::create_directories(userPath.parent_path(), ec);
	if (ec) {
		logger::error("[OAR] Failed to create user settings directory '{}': {}",
			userPath.parent_path().string(), ec.message());
	}

	if (!ec && ini.SaveFile(kUserSettingsPath) >= 0) {
		logger::info("[OAR] User settings saved to '{}' (defaults preserved at '{}')",
			kUserSettingsPath, kSettingsPath);
	} else {
		logger::error("[OAR] Failed to save user settings to '{}'", kUserSettingsPath);
	}
}
