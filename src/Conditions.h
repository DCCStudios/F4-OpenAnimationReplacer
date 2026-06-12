#pragma once

#include "BaseConditions.h"

struct FormComponent
{
	std::string pluginName;
	uint32_t localFormID{ 0 };
	RE::TESForm* cachedForm{ nullptr };

	void Initialize(const nlohmann::json& a_json);
	void Serialize(nlohmann::json& a_json) const;
	void ResolveForm();
	std::string GetDisplayString() const;
	void DrawEditWidgets(const char* a_label, bool& a_dirty, RE::ENUM_FORM_ID a_formTypeHint = RE::ENUM_FORM_ID::kNONE);
};

struct NumericComponent
{
	enum class ValueType : int32_t { kStatic, kGlobalVariable, kActorValue };

	ValueType type{ ValueType::kStatic };
	float staticValue{ 0.0f };
	FormComponent formComponent;
	std::string actorValueName;

	float GetValue(RE::TESObjectREFR* a_refr) const;
	void Initialize(const nlohmann::json& a_json);
	void Serialize(nlohmann::json& a_json) const;
};

// ===== Concrete conditions =====

class IsFormCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsForm"; }
	std::string GetDescription() const override { return "Checks if the reference matches a specific form. Use plugin name + local form ID."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Form", a_dirty); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class IsActorBaseCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsActorBase"; }
	std::string GetDescription() const override { return "Checks if the actor's base form (NPC_ record) matches. Use for targeting specific named NPCs."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Actor Base", a_dirty, RE::ENUM_FORM_ID::kNPC_); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class IsRaceCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsRace"; }
	std::string GetDescription() const override { return "Checks if the actor's race matches. Common: HumanRace, GhoulRace, SuperMutantRace, SynthGen1Race, etc."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Race", a_dirty, RE::ENUM_FORM_ID::kRACE); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class IsFemaleCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsFemale"; }
	std::string GetDescription() const override { return "Is the actor female?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsWeaponDrawnCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsWeaponDrawn"; }
	std::string GetDescription() const override { return "True when the actor has their weapon raised and ready. Checks the WeaponState flag."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsInCombatCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsInCombat"; }
	std::string GetDescription() const override { return "True when the actor is actively engaged in combat. See also IsCombatState for finer control (combat/searching)."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsSprintingCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsSprinting"; }
	std::string GetDescription() const override { return "Is the actor sprinting?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsInAirCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsInAir"; }
	std::string GetDescription() const override { return "True when the actor is airborne (jumping or falling). Combine with FallDistance for landing animations."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class HasKeywordCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "HasKeyword"; }
	std::string GetDescription() const override { return "Checks if the actor or their base form has a keyword. Enter keyword editor ID or use plugin:formID."; }
	std::string GetParameterString() const override { return editorID.empty() ? keywordForm.GetDisplayString() : editorID; }
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent keywordForm;
	std::string editorID;
	RE::BGSKeyword* cachedKeyword{ nullptr };
};

class IsInFactionCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsInFaction"; }
	std::string GetDescription() const override { return "Checks if the actor belongs to a specific faction. Use plugin:formID to specify the faction."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Faction", a_dirty, RE::ENUM_FORM_ID::kFACT); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class RandomCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "Random"; }
	std::string GetDescription() const override { return "Rolls a random number each evaluation. Set threshold 0.0 to 1.0 (e.g. 0.5 = 50% chance). Re-rolls each time conditions are checked."; }
	std::string GetParameterString() const override { return std::format("{:.0f}%", threshold * 100.f); }
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	float threshold{ 0.5f };
};

class LevelCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "Level"; }
	std::string GetDescription() const override { return "Compares the actor's current level. Value can be static, a global variable, or an actor value."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	NumericComponent numericValue;
};

class ORCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "OR"; }
	std::string GetDescription() const override { return "Any child condition passes (OR logic)"; }
	ConditionSet& GetConditionSet() { return childConditions; }
	const ConditionSet& GetConditionSet() const { return childConditions; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ConditionSet childConditions;
};

class ANDCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "AND"; }
	std::string GetDescription() const override { return "All child conditions pass (AND logic)"; }
	ConditionSet& GetConditionSet() { return childConditions; }
	const ConditionSet& GetConditionSet() const { return childConditions; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ConditionSet childConditions;
};

class IsEquippedTypeCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsEquippedType"; }
	std::string GetDescription() const override { return "Checks if the equipped weapon matches a weapon type keyword (e.g. WeaponTypePistol, WeaponTypeRifle, WeaponTypeLaser)."; }
	std::string GetParameterString() const override { return weaponKeywordForm.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { weaponKeywordForm.DrawEditWidgets("Weapon Keyword", a_dirty, RE::ENUM_FORM_ID::kKYWD); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent weaponKeywordForm;
	RE::BGSKeyword* cachedKeyword{ nullptr };
};

class IsInPowerArmorCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsInPowerArmor"; }
	std::string GetDescription() const override { return "Is the actor in power armor?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsSneakingCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsSneaking"; }
	std::string GetDescription() const override { return "Is the actor sneaking?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class CurrentWeatherCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "CurrentWeather"; }
	std::string GetDescription() const override { return "[STUB] Would check if the current weather matches a form. Sky singleton not exposed in FO4 CommonLibF4."; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "Sky singleton not exposed in CommonLibF4"; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Weather", a_dirty, RE::ENUM_FORM_ID::kWTHR); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class IsADSCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsADS"; }
	std::string GetDescription() const override { return "True when the actor is aiming down sights (or using a scope). Reads the 'IsAiming' graph variable."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class CompareActorValueCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "CompareActorValue"; }
	std::string GetDescription() const override { return "Compares an actor value (Health, ActionPoints, Strength, Perception, Rads, etc.) against a numeric value."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	std::string actorValueName;
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	NumericComponent numericValue;
	mutable RE::ActorValueInfo* cachedAVInfo{ nullptr };
	mutable bool avInfoResolved{ false };
};

class CurrentMagazineAmmoCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "CurrentMagazineAmmo"; }
	std::string GetDescription() const override { return "Compares ammo currently loaded in the equipped weapon's magazine. 0 = empty. Useful for empty reload animations."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	NumericComponent numericValue;
};

class IsEquippedHasKeywordCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsEquippedHasKeyword"; }
	std::string GetDescription() const override { return "Checks if the currently equipped weapon has a specific keyword. Enter keyword editor ID or plugin:formID."; }
	std::string GetParameterString() const override { return editorID.empty() ? keywordForm.GetDisplayString() : editorID; }
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent keywordForm;
	std::string editorID;
	RE::BGSKeyword* cachedKeyword{ nullptr };
};

class IsEquippedCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsEquipped"; }
	std::string GetDescription() const override { return "Checks if a specific weapon or item form is currently equipped by the actor."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Form", a_dirty, RE::ENUM_FORM_ID::kWEAP); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

// ===== XOR Logic Gate =====

class XORCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "XOR"; }
	std::string GetDescription() const override { return "Exactly one child condition passes (XOR logic)"; }
	ConditionSet& GetConditionSet() { return childConditions; }
	const ConditionSet& GetConditionSet() const { return childConditions; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ConditionSet childConditions;
};

// ===== Identity Conditions =====

class IsChildCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsChild"; }
	std::string GetDescription() const override { return "Is the actor a child?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsPlayerTeammateCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsPlayerTeammate"; }
	std::string GetDescription() const override { return "True if the actor is currently a companion or follower of the player."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsTalkingCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsTalking"; }
	std::string GetDescription() const override { return "Is the actor currently talking?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsAttackingCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsAttacking"; }
	std::string GetDescription() const override { return "True during any attack: melee (meleeAttackState != 0) OR gun firing (gunState 7/8). Covers all combat actions."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsReloadingCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsReloading"; }
	std::string GetDescription() const override { return "True when the actor is reloading their weapon. Checks gunState == 4 (kReloading)."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsFiringCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsFiring"; }
	std::string GetDescription() const override { return "True when the actor is firing a ranged weapon. Checks gunState 7 (kFire) or 8 (kFireSighted)."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

// IsDryFiring — true when the engine dispatches ActionFireEmpty to this actor
// (i.e., the player pressed fire with no ammo). Pure input detection only.
class IsDryFiringCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsDryFiring"; }
	std::string GetDescription() const override { return "True when the engine fires the ActionFireEmpty event (player pressed fire with no ammo). Pure input detection — combine with CurrentMagazineAmmo, IsWeaponDrawn, etc. as needed."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

// IsButtonHeld — true when a specific user event's mapped button is currently pressed.
// User events: "Attack", "Activate", "Sprint", "Jump", "Sneak", "Block", "Ready",
// "Reload", "Bash", "ADS", "Melee", "Grenade", "VATS", "Favorites", etc.
class IsButtonHeldCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsButtonHeld"; }
	std::string GetDescription() const override { return "True when the specified user event button is currently held. Enter the event name (e.g. 'Attack', 'Sprint', 'Activate', 'Jump', 'Reload')."; }
	std::string GetParameterString() const override { return std::format("'{}'", userEvent); }
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	std::string userEvent;
};

class IsBlockingCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsBlocking"; }
	std::string GetDescription() const override { return "True when the actor is actively blocking. Reads the 'IsBlocking' graph variable."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsRunningCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsRunning"; }
	std::string GetDescription() const override { return "True when the actor is running (not walking or sprinting). Based on movement speed thresholds."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsInInteriorCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsInInterior"; }
	std::string GetDescription() const override { return "True when the actor is in an interior cell (buildings, vaults, interiors, etc.)."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsWorldSpaceCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsWorldSpace"; }
	std::string GetDescription() const override { return "Checks if the actor is in a specific worldspace (e.g. Commonwealth, TheGlowingSea, FarHarbor)."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("WorldSpace", a_dirty, RE::ENUM_FORM_ID::kWRLD); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class IsParentCellCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsParentCell"; }
	std::string GetDescription() const override { return "Checks if the actor's parent cell matches a specific cell form. Works for both interior and exterior cells."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Cell", a_dirty, RE::ENUM_FORM_ID::kCELL); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class IsInLocationCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsInLocation"; }
	std::string GetDescription() const override { return "Checks if the actor is in a specific location. Also matches parent locations in the location hierarchy."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Location", a_dirty, RE::ENUM_FORM_ID::kLCTN); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class HasPerkCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "HasPerk"; }
	std::string GetDescription() const override { return "Checks if the actor has a perk, including both base form perks and perks added at runtime (via leveling/scripts)."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Perk", a_dirty, RE::ENUM_FORM_ID::kPERK); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class HasSpellCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "HasSpell"; }
	std::string GetDescription() const override { return "Checks if the actor has a specific spell in their spell list (includes abilities and lesser powers)."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Spell", a_dirty, RE::ENUM_FORM_ID::kSPEL); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class ScaleCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "Scale"; }
	std::string GetDescription() const override { return "Compares the actor's visual scale multiplier. 1.0 = normal size. Set via SetScale or CK."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	NumericComponent numericValue;
};

class CurrentGameTimeCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "CurrentGameTime"; }
	std::string GetDescription() const override { return "Compares the current in-game hour of day (0.0 - 23.99). Use for time-of-day dependent animations."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	NumericComponent numericValue;
};

class FactionRankCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "FactionRank"; }
	std::string GetDescription() const override { return "Compares the actor's rank within a faction. -1 = not a member. 0+ = rank index."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent factionForm;
	ComparisonOperator comparison{ ComparisonOperator::kGreaterEqual };
	NumericComponent numericValue;
};

class CrimeGoldCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "CrimeGold"; }
	std::string GetDescription() const override { return "Compares the actor's accumulated crime gold (bounty) with a specific faction."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent factionForm;
	ComparisonOperator comparison{ ComparisonOperator::kGreaterEqual };
	NumericComponent numericValue;
};

class LifeStateCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "LifeState"; }
	std::string GetDescription() const override { return "Compares actor life state. Values: 0=Alive, 1=Dying, 2=Dead, 3=Unconscious, 4=Reanimate, 5=Recycle, 6=Restrained, 7=EssentialDown, 8=Bleedout."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	NumericComponent numericValue;
};

class SitSleepStateCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "SitSleepState"; }
	std::string GetDescription() const override { return "Compares sit/sleep state. 0=Normal, 3=Sitting, 4=Wants to sleep, 6=Sleeping. Other values are transition states."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	NumericComponent numericValue;
};

class HasTargetCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "HasTarget"; }
	std::string GetDescription() const override { return "Does the actor have a combat target?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class CurrentTargetDistanceCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "CurrentTargetDistance"; }
	std::string GetDescription() const override { return "Compares distance to the combat target in game units. Approximately 70 units = 1 meter. Requires an active target."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kLess };
	NumericComponent numericValue;
};

class IsMovementDirectionCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsMovementDirection"; }
	std::string GetDescription() const override { return "Checks movement direction relative to facing. 0=Forward, 1=Right, 2=Backward, 3=Left. Uses the 'Direction' graph variable."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	int32_t direction{ 0 };
};

class IsCombatStateCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsCombatState"; }
	std::string GetDescription() const override { return "Compares combat state. 0=Not in combat, 1=In combat (hostile target visible), 2=Searching (lost sight of target)."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	NumericComponent numericValue;
};

// ===== Priority 2 Conditions =====

class HasGraphVariableCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "HasGraphVariable"; }
	std::string GetDescription() const override { return "Checks a Havok behavior graph variable by name. Supports float, int, and bool types. Powerful for custom behavior checks."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	std::string variableName;
	enum class VarType { kFloat, kInt, kBool } varType{ VarType::kFloat };
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	float floatValue{ 0.f };
	int32_t intValue{ 0 };
	bool boolValue{ false };
};

class IsReplacerEnabledCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsReplacerEnabled"; }
	std::string GetDescription() const override { return "True if another OAR replacer mod or submod is currently enabled. Useful for inter-mod dependencies and overrides."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	std::string modName;
	std::string subModName;
};

class MovementSpeedCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "MovementSpeed"; }
	std::string GetDescription() const override { return "Compares the actor's current movement speed in game units per second. 0 = standing still."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kGreater };
	NumericComponent numericValue;
};

class IsSwimmingCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsSwimming"; }
	std::string GetDescription() const override { return "Is the actor swimming?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsOverEncumberedCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsOverEncumbered"; }
	std::string GetDescription() const override { return "True when the actor's carry weight exceeds their maximum. Affects movement speed and AP drain."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class LocationHasKeywordCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "LocationHasKeyword"; }
	std::string GetDescription() const override { return "Checks if the actor's current location has a specific keyword (e.g. LocTypeSettlement, LocTypeDungeon)."; }
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	std::string editorID;
	RE::BGSKeyword* cachedKeyword{ nullptr };
};

class LocationClearedCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "LocationCleared"; }
	std::string GetDescription() const override { return "True if the actor's current location has been marked as cleared (all enemies defeated)."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

// ===== Stub Conditions (Hard - no reliable API) =====

class LightLevelCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "LightLevel"; }
	std::string GetDescription() const override { return "[STUB] Would compare the ambient light level at the actor's position. Not available in FO4 CommonLibF4."; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "No GetLightLevel API in FO4 CommonLibF4"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const override { return false; }
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class SurfaceMaterialCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "SurfaceMaterial"; }
	std::string GetDescription() const override { return "[STUB] Would check the ground material type under the actor (metal, dirt, wood, etc.). No API available."; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "No ground material accessor in FO4 CommonLibF4"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const override { return false; }
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class MovementSurfaceAngleCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "MovementSurfaceAngle"; }
	std::string GetDescription() const override { return "[STUB] Would compare the slope angle of the ground under the actor. Requires raycast - no API available."; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "Requires raycast - not available in FO4 CommonLibF4"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const override { return false; }
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsOnStairsCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsOnStairs"; }
	std::string GetDescription() const override { return "[STUB] Would detect if the actor is currently on stairs. No stairs detection API found in FO4."; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "No stairs detection API found in FO4"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const override { return false; }
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class WindSpeedCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "WindSpeed"; }
	std::string GetDescription() const override { return "[STUB] Would compare current wind speed. Weather wind data not yet reverse-engineered for FO4."; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "TESWeather wind data layout not reverse-engineered for FO4"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const override { return false; }
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class WindAngleDifferenceCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "WindAngleDifference"; }
	std::string GetDescription() const override { return "[STUB] Would compare the angle between wind direction and actor facing. Weather wind data not available."; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "TESWeather wind data layout not reverse-engineered for FO4"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const override { return false; }
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

// ===== Priority 3 Conditions =====

class IsTrespassingCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsTrespassing"; }
	std::string GetDescription() const override { return "Is the actor trespassing?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsCurrentPackageCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsCurrentPackage"; }
	std::string GetDescription() const override { return "Checks if the actor is currently running a specific AI package. Useful for detecting sandbox, travel, patrol, etc."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Package", a_dirty, RE::ENUM_FORM_ID::kPACK); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class IsInSceneCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsInScene"; }
	std::string GetDescription() const override { return "True if the actor is participating in a scene (dialogue, scripted events, quest sequences)."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class CurrentFurnitureCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "CurrentFurniture"; }
	std::string GetDescription() const override { return "Checks if the actor is using a specific furniture form (workbenches, chairs, beds, terminals, etc.)."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Furniture", a_dirty, RE::ENUM_FORM_ID::kFURN); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

// ===== Context Wrappers =====

class TargetConditionWrapper : public ConditionBase
{
public:
	std::string GetName() const override { return "TARGET"; }
	std::string GetDescription() const override { return "Context wrapper: evaluates all child conditions against the actor's combat target instead of the actor itself."; }
	ConditionSet& GetConditionSet() { return childConditions; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ConditionSet childConditions;
};

class PlayerConditionWrapper : public ConditionBase
{
public:
	std::string GetName() const override { return "PLAYER"; }
	std::string GetDescription() const override { return "Context wrapper: evaluates all child conditions against the player character instead of the current actor."; }
	ConditionSet& GetConditionSet() { return childConditions; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ConditionSet childConditions;
};

// ===== Additional Missing Conditions =====

class IsUniqueCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsUnique"; }
	std::string GetDescription() const override { return "True if the NPC has the 'Unique' flag in their base form. Named/important NPCs are typically unique."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsSummonedCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsSummoned"; }
	std::string GetDescription() const override { return "True if the actor was summoned/conjured (has the Summonable flag in their base form)."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsGhostCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsGhost"; }
	std::string GetDescription() const override { return "True if the actor has the Ghost flag (invulnerable, projectiles pass through). Set in CK or via scripts."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsGreetingPlayerCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsGreetingPlayer"; }
	std::string GetDescription() const override { return "True when an NPC is actively greeting the player (hello dialogue). Checks PlayerCharacter::greetingPlayer."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsGuardCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsGuard"; }
	std::string GetDescription() const override { return "True if the NPC is flagged as a guard in their AI process data (hostileGuard flag)."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class HeightCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "Height"; }
	std::string GetDescription() const override { return "Compares the actor's height/bounding box size in game units. Uses GetActorHeightOrRefBound."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	NumericComponent numericValue;
};

class WeightCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "Weight"; }
	std::string GetDescription() const override { return "Compares the total weight of all items currently equipped by the actor (weapons + armor)."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	NumericComponent numericValue;
};

class InventoryCountCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "InventoryCount"; }
	std::string GetDescription() const override { return "Compares the count of a specific item in the actor's inventory. Specify the item form and a comparison value."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent itemForm;
	ComparisonOperator comparison{ ComparisonOperator::kGreaterEqual };
	NumericComponent numericValue;
};

class EquippedObjectWeightCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "EquippedObjectWeight"; }
	std::string GetDescription() const override { return "Compares the weight of the currently equipped weapon or item specifically (not total equipped weight)."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kGreater };
	NumericComponent numericValue;
};

class InventoryWeightCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "InventoryWeight"; }
	std::string GetDescription() const override { return "Compares the total weight of all items in the actor's inventory (sum of all stack weights)."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kGreater };
	NumericComponent numericValue;
};

class SubmergeLevelCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "SubmergeLevel"; }
	std::string GetDescription() const override { return "Compares how deep the actor is submerged in water. 0.0 = surface, 1.0 = fully submerged."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kGreater };
	NumericComponent numericValue;
};

class HasMagicEffectCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "HasMagicEffect"; }
	std::string GetDescription() const override { return "Checks if the actor currently has an active magic effect (from potions, spells, radiation, etc.)."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("MagicEffect", a_dirty, RE::ENUM_FORM_ID::kMGEF); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class IsWornCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsWorn"; }
	std::string GetDescription() const override { return "True if a specific item form is currently equipped/worn by the actor. Checks all equipped slots."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Item", a_dirty); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

class IsWornHasKeywordCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsWornHasKeyword"; }
	std::string GetDescription() const override { return "True if any currently equipped weapon or armor has the specified keyword. Enter keyword editor ID."; }
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	std::string editorID;
	RE::BGSKeyword* cachedKeyword{ nullptr };
};

class IsDoingFavorCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsDoingFavor"; }
	std::string GetDescription() const override { return "True when the NPC is performing a favor/command for the player (e.g. 'go here', 'pick that up')."; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IdleTimeCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IdleTime"; }
	std::string GetDescription() const override { return "Compares seconds since the actor last moved or performed an action. Uses packageIdleTimer from AI process."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kGreater };
	NumericComponent numericValue;
};

class CurrentTargetAngleCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "CurrentTargetAngle"; }
	std::string GetDescription() const override { return "Compares the absolute angle (degrees) between the actor's facing direction and their combat target. 0 = facing directly, 180 = behind."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kLess };
	NumericComponent numericValue;
};

class FallDistanceCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "FallDistance"; }
	std::string GetDescription() const override { return "Compares how far the actor has fallen in game units. Uses the 'FallDistance' graph variable. Useful for landing animations."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kGreater };
	NumericComponent numericValue;
};

class IsQuestStageDoneCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsQuestStageDone"; }
	std::string GetDescription() const override { return "True if a specific quest stage has been completed. Specify the quest form and the stage number to check."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent questForm;
	int32_t stage{ 0 };
};

// ===== Animation Time Conditions =====

class AnimTimeRemainingCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "AnimTimeRemaining"; }
	std::string GetDescription() const override { return "Compares the time remaining (seconds) in the current clip's animation. True while remaining time passes the comparison. When it fails, the replacement's conditions become false and blend-out begins."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kGreaterEqual };
	NumericComponent numericValue;
};

class AnimTimeElapsedCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "AnimTimeElapsed"; }
	std::string GetDescription() const override { return "Compares the elapsed time (seconds) since the current clip's animation started playing. Reads localTime from the hkbClipGenerator."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kGreaterEqual };
	NumericComponent numericValue;
};

class AnimProgressCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "AnimProgress"; }
	std::string GetDescription() const override { return "Compares the animation progress as a ratio (0.0 = start, 1.0 = end). Useful for triggering at a percentage of the animation rather than absolute seconds."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kLess };
	NumericComponent numericValue;
};

class IsPlayingIdleAnimationCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsPlayingIdleAnimation"; }
	std::string GetDescription() const override { return "Checks if the actor is currently playing a specific idle animation (TESIdleForm). Uses currentIdle from the AI process."; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Idle Animation", a_dirty, RE::ENUM_FORM_ID::kIDLE); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

// =============================================================================
// Detection Conditions — ported from Skyrim OAR Detection Conditions plugin
// =============================================================================

// Thread-local context: set by DetectedBy/Detects during iteration, read by
// child-only conditions (DetectionDistance, DetectionRelationship, DetectionAngle)
inline thread_local RE::Actor* g_detectionTarget = nullptr;
inline thread_local RE::Actor* g_detectionSource = nullptr;

class DetectedByCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "DetectedBy"; }
	std::string GetDescription() const override { return "True if an actor that passes all child conditions detects the evaluated actor (stealth detection)."; }
	ConditionSet& GetConditionSet() { return childConditions; }
	const ConditionSet& GetConditionSet() const { return childConditions; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ConditionSet childConditions;
};

class DetectsCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "Detects"; }
	std::string GetDescription() const override { return "True if the evaluated actor detects an actor that passes all child conditions (stealth detection)."; }
	ConditionSet& GetConditionSet() { return childConditions; }
	const ConditionSet& GetConditionSet() const { return childConditions; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ConditionSet childConditions;
};

class DetectionDistanceCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "DetectionDistance"; }
	std::string GetDescription() const override { return "Compares distance between detector and target. Only meaningful inside DetectedBy/Detects conditions."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kLess };
	NumericComponent numericValue;
};

class DetectionRelationshipCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "DetectionRelationship"; }
	std::string GetDescription() const override { return "Compares relationship rank between detector and target (-4 Archnemesis to +4 Lover). Only meaningful inside DetectedBy/Detects conditions."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	ComparisonOperator comparison{ ComparisonOperator::kEqual };
	NumericComponent numericValue;
};

class DetectionAngleCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "DetectionAngle"; }
	std::string GetDescription() const override { return "Compares angle from one actor to another (degrees). Options to swap perspective and limit to left/right side. Only meaningful inside DetectedBy/Detects conditions."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	bool swapActors{ false };
	ComparisonOperator comparison{ ComparisonOperator::kLess };
	NumericComponent numericValue;
	bool limitRight{ false };
	bool limitLeft{ false };
};

// =============================================================================
// Dialogue Condition — ported from Skyrim OAR Dialogue Conditions plugin
// =============================================================================

class DialogueCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "Dialogue"; }
	std::string GetDescription() const override { return "Detects dialogue state phases. Enable sub-checks for the phases you want to match. Uses edge detection for transient states (started, chose, ended)."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	// Which sub-checks are enabled (true = require this state to match)
	bool checkDialogueActive{ false };
	bool checkDialogueStarted{ false };
	bool checkPlayerChoosing{ false };
	bool checkPlayerChose{ false };
	bool checkDialogueEnded{ false };

	// Mutable state tracking for edge detection
	mutable bool prevMenuOpen{ false };
	mutable bool dialogueStartedFlag{ false };
	mutable bool playerChoseFlag{ false };
	mutable bool dialogueEndedFlag{ false };
};

// =============================================================================
// Math Condition — ported from Skyrim OAR Math plugin
// =============================================================================

struct MathVariable
{
	std::string name;
	NumericComponent numericValue;
};

class MathStatementCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "MathStatement"; }
	std::string GetDescription() const override { return "Evaluates a mathematical expression. Variables are bound to numeric values (static, global, actor value). Expression is true when result != 0."; }
	std::string GetParameterString() const override;
	void DrawEditWidgets(bool& a_dirty) override;
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	std::string expression;
	std::vector<MathVariable> variables;

	// Cached compiled expression state (mutable for const evaluate)
	mutable bool needsRecompile{ true };
	mutable std::string lastExpression;
};

void RegisterAllConditions();
std::unique_ptr<ICondition> CreateConditionFromJson(const nlohmann::json& a_json);
