#pragma once

#include "HavokTypes.h"
#include "Utils.h"
#include "ReplacementAnimation.h"
#include "ReplacerMods.h"

class ReplacerProjectData;

struct ReplacementAnimFileInfo
{
	std::string originalPath;
	std::string replacementPath;
	std::string absoluteDiskPath;
	SubMod* parentSubMod{ nullptr };
	ReplacementAnimation* replacementAnim{ nullptr };
};

class OpenAnimationReplacer
{
public:
	static OpenAnimationReplacer* GetSingleton()
	{
		static OpenAnimationReplacer singleton;
		return &singleton;
	}

	void AddReplacerMod(std::unique_ptr<ReplacerMod> a_mod);
	void ClearAllMods();

	ReplacementAnimation* GetReplacementAnimation(
		RE::hkbClipGenerator* a_clipGen,
		int16_t a_originalIndex,
		RE::TESObjectREFR* a_refr);

	ReplacerProjectData* GetReplacerProjectData(RE::hkbCharacterStringData* a_stringData);
	ReplacerProjectData& GetOrCreateReplacerProjectData(RE::hkbCharacterStringData* a_stringData);

	void AddOwnedAnimation(std::unique_ptr<ReplacementAnimation> a_anim);

	void AddReplacementFileInfo(const std::string& a_normalizedOrigPath,
		const ReplacementAnimFileInfo& a_info);

	bool CreateReplacementAnimations(const char* a_animationPath,
		RE::hkbCharacterStringData* a_stringData);

	const std::vector<std::unique_ptr<ReplacerMod>>& GetReplacerMods() const { return replacerMods; }
	std::shared_mutex& GetModsMutex() const { return modsMutex; }
	size_t GetTotalReplacementCount() const;

	const auto& GetPathToSubModsMap() const { return animPathToReplacementsMap; }
	bool HasReplacementsForPath(const std::string& a_normalizedPath) const;

	std::atomic<int> loadingTotalAnims{ 0 };
	std::atomic<int> loadingParsedAnims{ 0 };
	std::atomic<int> loadingLoadedAnims{ 0 };
	std::atomic<bool> isLoading{ false };
	std::atomic<bool> loadingComplete{ false };
	std::string loadingPhase{ "Idle" };

private:
	OpenAnimationReplacer() = default;

	mutable std::shared_mutex modsMutex;
	std::vector<std::unique_ptr<ReplacerMod>> replacerMods;

	mutable std::shared_mutex projectDataMutex;
	std::unordered_map<RE::hkbCharacterStringData*, std::unique_ptr<ReplacerProjectData>> projectDataMap;

	mutable std::shared_mutex ownedAnimsMutex;
	std::vector<std::unique_ptr<ReplacementAnimation>> ownedAnimations;

	mutable std::shared_mutex pathMapMutex;
	std::unordered_map<std::string, std::vector<ReplacementAnimFileInfo>> animPathToReplacementsMap;
};
