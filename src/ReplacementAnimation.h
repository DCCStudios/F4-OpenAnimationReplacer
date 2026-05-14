#pragma once

#include "Utils.h"
#include "HavokTypes.h"
#include "BaseConditions.h"
#include "Variants.h"

class SubMod;

class ReplacementAnimation
{
public:
	ReplacementAnimation(std::string a_originalPath, std::string a_replacementPath,
		int16_t a_bindingIndex, SubMod* a_parentSubMod)
		: originalPath(std::move(a_originalPath))
		, replacementPath(std::move(a_replacementPath))
		, bindingIndex(a_bindingIndex)
		, parentSubMod(a_parentSubMod)
	{}

	const std::string& GetOriginalPath() const { return originalPath; }
	const std::string& GetReplacementPath() const { return replacementPath; }
	int16_t GetBindingIndex() const { return bindingIndex; }
	SubMod* GetParentSubMod() const { return parentSubMod; }
	bool HasVariants() const { return variants != nullptr; }
	Variants* GetVariants() const { return variants.get(); }
	ConditionSet* GetPerAnimConditionSet() const { return perAnimConditions.get(); }

	void SetVariants(std::unique_ptr<Variants> a_variants) { variants = std::move(a_variants); }
	void SetPerAnimConditions(std::unique_ptr<ConditionSet> a_conds) { perAnimConditions = std::move(a_conds); }
	void SetBindingIndex(int16_t a_index) { bindingIndex = a_index; }

private:
	std::string originalPath;
	std::string replacementPath;
	int16_t bindingIndex{ -1 };
	SubMod* parentSubMod{ nullptr };
	std::unique_ptr<Variants> variants;
	std::unique_ptr<ConditionSet> perAnimConditions;
};

class AnimationReplacements
{
public:
	void AddReplacement(ReplacementAnimation* a_replacement);
	void SortByPriority();
	ReplacementAnimation* EvaluateConditionsAndGetReplacement(RE::TESObjectREFR* a_refr,
		RE::hkbClipGenerator* a_clipGen) const;

	mutable std::shared_mutex mutex;

private:
	std::vector<ReplacementAnimation*> replacements;
};

class ReplacerProjectData
{
public:
	AnimationReplacements* GetReplacements(int16_t a_originalIndex);
	AnimationReplacements& GetOrCreateReplacements(int16_t a_originalIndex);
	int16_t GetOriginalIndex(int16_t a_replacementIndex) const;
	void SetOriginalIndex(int16_t a_replacementIndex, int16_t a_originalIndex);

	mutable std::shared_mutex mutex;

private:
	std::unordered_map<int16_t, std::unique_ptr<AnimationReplacements>> replacementsMap;
	std::unordered_map<int16_t, int16_t> replacementIndexToOriginalIndex;
};
