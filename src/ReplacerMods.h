#pragma once

#include "Utils.h"
#include "HavokTypes.h"
#include "BaseConditions.h"

class ReplacementAnimation;

struct ConditionPreset
{
	std::string name;
	std::string description;
	std::unique_ptr<ConditionSet> conditions;
};

class SubMod
{
public:
	SubMod(const std::string& a_name, int32_t a_priority, const std::filesystem::path& a_path)
		: name(a_name), priority(a_priority), path(a_path) {}

	const std::string& GetName() const { return name; }
	const std::string& GetDescription() const { return description; }
	int32_t GetPriority() const { return priority; }
	bool IsDisabled() const { return disabled; }
	bool IsInterruptible() const { return interruptible; }
	bool GetReplaceOnLoop() const { return replaceOnLoop; }
	bool GetReplaceOnEcho() const { return replaceOnEcho; }
	bool GetKeepRandomResultsOnLoop() const { return keepRandomResultsOnLoop; }
	bool GetShareRandomResults() const { return shareRandomResults; }
	float GetCustomBlendTimeOnInterrupt() const { return customBlendTimeOnInterrupt; }
	float GetCustomBlendTimeOnLoop() const { return customBlendTimeOnLoop; }
	float GetCustomBlendTimeOnEcho() const { return customBlendTimeOnEcho; }
	const std::string& GetRequiredProjectName() const { return requiredProjectName; }
	const std::string& GetOverrideAnimFolder() const { return overrideAnimationsFolder; }
	const std::filesystem::path& GetPath() const { return path; }
	ConditionSet* GetConditionSet() const { return conditionSet.get(); }
	const std::vector<ReplacementAnimation*>& GetReplacementAnimations() const { return replacementAnimations; }
	bool IsDirty() const { return dirty; }

	void SetName(const std::string& a_name) { name = a_name; }
	void SetDescription(const std::string& a_desc) { description = a_desc; }
	void SetPriority(int32_t a_priority) { priority = a_priority; }
	void SetDisabled(bool a_disabled) { disabled = a_disabled; }
	void SetInterruptible(bool a_val) { interruptible = a_val; }
	void SetReplaceOnLoop(bool a_val) { replaceOnLoop = a_val; }
	void SetReplaceOnEcho(bool a_val) { replaceOnEcho = a_val; }
	void SetKeepRandomResultsOnLoop(bool a_val) { keepRandomResultsOnLoop = a_val; }
	void SetShareRandomResults(bool a_val) { shareRandomResults = a_val; }
	void SetDirty(bool a_dirty) { dirty = a_dirty; }
	void SetConditionSet(std::unique_ptr<ConditionSet> a_set) { conditionSet = std::move(a_set); }
	void AddReplacementAnimation(ReplacementAnimation* a_anim) { replacementAnimations.push_back(a_anim); }

	bool EvaluateConditions(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) const;

	std::string name;
	std::string description;
	int32_t priority{ 0 };
	bool disabled{ false };
	bool interruptible{ false };
	bool replaceOnLoop{ true };
	bool replaceOnEcho{ true };
	bool keepRandomResultsOnLoop{ false };
	bool shareRandomResults{ false };
	float customBlendTimeOnInterrupt{ -1.0f };
	float customBlendTimeOnLoop{ -1.0f };
	float customBlendTimeOnEcho{ -1.0f };
	std::string requiredProjectName;
	std::string overrideAnimationsFolder;
	std::filesystem::path path;
	bool dirty{ false };
	bool hasUserConfig{ false };

	std::unique_ptr<ConditionSet> conditionSet;
	std::vector<ReplacementAnimation*> replacementAnimations;
};

class ReplacerMod
{
public:
	ReplacerMod(const std::string& a_name, const std::filesystem::path& a_path)
		: name(a_name), path(a_path) {}

	const std::string& GetName() const { return name; }
	const std::string& GetAuthor() const { return author; }
	const std::string& GetDescription() const { return description; }
	const std::filesystem::path& GetPath() const { return path; }

	void SetName(const std::string& a_name) { name = a_name; }
	void SetAuthor(const std::string& a_author) { author = a_author; }
	void SetDescription(const std::string& a_desc) { description = a_desc; }

	void AddSubMod(std::unique_ptr<SubMod> a_subMod)
	{
		subMods.push_back(std::move(a_subMod));
	}

	void SortSubMods()
	{
		std::ranges::sort(subMods, [](const auto& a, const auto& b) {
			return a->GetPriority() > b->GetPriority();
		});
	}

	const std::vector<std::unique_ptr<SubMod>>& GetSubMods() const { return subMods; }

	std::string name;
	std::string author;
	std::string description;
	std::filesystem::path path;
	std::vector<ConditionPreset> conditionPresets;
	std::vector<std::unique_ptr<SubMod>> subMods;
};
