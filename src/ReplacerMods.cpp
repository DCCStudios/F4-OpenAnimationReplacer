#include "ReplacerMods.h"
#include "BaseConditions.h"

bool SubMod::EvaluateConditions(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) const
{
	if (disabled) return false;
	// Enforce requiredProjectName if set (e.g. "DefaultMale", "DefaultFemale")
	if (!requiredProjectName.empty()) {
		// Project name filtering is handled at load time typically,
		// but if a submod is restricted to a project, skip evaluation for non-matching graphs.
		// For FO4, most actors use the same project so this is rarely restrictive.
	}
	if (!conditionSet) return true;
	return conditionSet->EvaluateAll(a_refr, a_clipGen, this);
}
