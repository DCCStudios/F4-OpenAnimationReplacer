#include "ReplacerMods.h"
#include "BaseConditions.h"

bool SubMod::EvaluateConditions(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) const
{
	if (disabled) return false;
	if (!conditionSet) return true;
	return conditionSet->EvaluateAll(a_refr, a_clipGen, this);
}
