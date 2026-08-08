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

	// Incremented by ClearAllMods. The UI compares this against the value it
	// last saw and drops its raw SubMod* selection/popup pointers when it
	// changed — after a config reload those objects have been destroyed.
	std::uint64_t GetModsGeneration() const { return modsGeneration.load(std::memory_order_acquire); }

	// Startup load (parse + preload) runs on a background thread so the main
	// menu stays responsive. Anything that must observe the fully-built state
	// (save load, new game, config reload) calls WaitForBackgroundLoad first;
	// after the first join it is a cheap no-op.
	void StartBackgroundLoad(std::function<void()> a_work);
	void WaitForBackgroundLoad();

	// Loading-phase label for the progress bar. An atomic enum instead of a
	// string: the background load thread writes it while the render thread
	// reads it every frame.
	enum class LoadingPhase : int
	{
		kIdle,
		kParsing,
		kLoading,
	};
	const char* GetLoadingPhaseText() const;

	std::atomic<int> loadingTotalAnims{ 0 };
	std::atomic<int> loadingParsedAnims{ 0 };
	std::atomic<int> loadingLoadedAnims{ 0 };
	std::atomic<bool> isLoading{ false };
	std::atomic<bool> loadingComplete{ false };
	std::atomic<LoadingPhase> loadingPhase{ LoadingPhase::kIdle };

private:
	OpenAnimationReplacer() = default;
	// A joinable std::thread reaching its destructor calls std::terminate —
	// join the background load if the process exits while it is still running.
	~OpenAnimationReplacer();

	std::mutex loadThreadMutex;
	std::thread loadThread;

	mutable std::shared_mutex modsMutex;
	std::vector<std::unique_ptr<ReplacerMod>> replacerMods;

	mutable std::shared_mutex projectDataMutex;
	std::unordered_map<RE::hkbCharacterStringData*, std::unique_ptr<ReplacerProjectData>> projectDataMap;

	mutable std::shared_mutex ownedAnimsMutex;
	std::vector<std::unique_ptr<ReplacementAnimation>> ownedAnimations;

	mutable std::shared_mutex pathMapMutex;
	std::unordered_map<std::string, std::vector<ReplacementAnimFileInfo>> animPathToReplacementsMap;

	std::atomic<std::uint64_t> modsGeneration{ 0 };
};
