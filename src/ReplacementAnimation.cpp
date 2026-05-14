#include "ReplacementAnimation.h"
#include "ReplacerMods.h"

void AnimationReplacements::AddReplacement(ReplacementAnimation* a_replacement)
{
	WriteLocker lock(mutex);
	replacements.push_back(a_replacement);
}

void AnimationReplacements::SortByPriority()
{
	WriteLocker lock(mutex);
	std::ranges::sort(replacements, [](const auto* a, const auto* b) {
		if (!a || !a->GetParentSubMod()) return false;
		if (!b || !b->GetParentSubMod()) return true;
		return a->GetParentSubMod()->GetPriority() > b->GetParentSubMod()->GetPriority();
	});
}

ReplacementAnimation* AnimationReplacements::EvaluateConditionsAndGetReplacement(
	RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) const
{
	ReadLocker lock(mutex);
	for (auto* replacement : replacements) {
		if (!replacement || !replacement->GetParentSubMod()) continue;
		if (replacement->GetParentSubMod()->IsDisabled()) continue;

		try {
			if (replacement->GetParentSubMod()->EvaluateConditions(a_refr, a_clipGen)) {
				if (auto* perAnim = replacement->GetPerAnimConditionSet()) {
					if (!perAnim->EvaluateAll(a_refr, a_clipGen, replacement->GetParentSubMod()))
						continue;
				}
				return replacement;
			}
		} catch (const std::exception& e) {
			logger::error("[OAR] Exception evaluating conditions for '{}': {}",
				replacement->GetParentSubMod()->GetName(), e.what());
		}
	}
	return nullptr;
}

AnimationReplacements* ReplacerProjectData::GetReplacements(int16_t a_originalIndex)
{
	ReadLocker lock(mutex);
	auto it = replacementsMap.find(a_originalIndex);
	return it != replacementsMap.end() ? it->second.get() : nullptr;
}

AnimationReplacements& ReplacerProjectData::GetOrCreateReplacements(int16_t a_originalIndex)
{
	WriteLocker lock(mutex);
	auto& ptr = replacementsMap[a_originalIndex];
	if (!ptr) {
		ptr = std::make_unique<AnimationReplacements>();
	}
	return *ptr;
}

int16_t ReplacerProjectData::GetOriginalIndex(int16_t a_replacementIndex) const
{
	ReadLocker lock(mutex);
	auto it = replacementIndexToOriginalIndex.find(a_replacementIndex);
	return it != replacementIndexToOriginalIndex.end() ? it->second : a_replacementIndex;
}

void ReplacerProjectData::SetOriginalIndex(int16_t a_replacementIndex, int16_t a_originalIndex)
{
	WriteLocker lock(mutex);
	replacementIndexToOriginalIndex[a_replacementIndex] = a_originalIndex;
}
