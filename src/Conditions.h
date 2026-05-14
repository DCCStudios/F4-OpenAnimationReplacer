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
	void DrawEditWidgets(const char* a_label, bool& a_dirty);
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
	std::string GetDescription() const override { return "Is the reference a specific form?"; }
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
	std::string GetDescription() const override { return "Is the actor's base a specific form?"; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Actor Base", a_dirty); }
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
	std::string GetDescription() const override { return "Is the actor a specific race?"; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Race", a_dirty); }
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
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "TESNPC class not fully defined in CommonLibF4"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsWeaponDrawnCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsWeaponDrawn"; }
	std::string GetDescription() const override { return "Does the actor have weapon/magic drawn?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class IsInCombatCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "IsInCombat"; }
	std::string GetDescription() const override { return "Is the actor in combat?"; }
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
	std::string GetDescription() const override { return "Is the actor in the air?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class HasKeywordCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "HasKeyword"; }
	std::string GetDescription() const override { return "Does the actor have a specific keyword?"; }
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
	std::string GetDescription() const override { return "Is the actor in a specific faction?"; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Faction", a_dirty); }
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
	std::string GetDescription() const override { return "Random chance (0.0-1.0 threshold)"; }
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
	std::string GetDescription() const override { return "Compare actor level"; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "No reliable level accessor without TESNPC class definition"; }
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
	std::string GetDescription() const override { return "Check equipped weapon type via keywords"; }
	std::string GetParameterString() const override { return weaponKeywordForm.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { weaponKeywordForm.DrawEditWidgets("Weapon Keyword", a_dirty); }
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
	std::string GetDescription() const override { return "Is the current weather a specific form?"; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "Sky singleton not exposed in CommonLibF4"; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Weather", a_dirty); }
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
	std::string GetDescription() const override { return "Is the actor aiming down sights?"; }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json&) override {}
	void SerializeImpl(nlohmann::json&) const override {}
};

class CompareActorValueCondition : public ConditionBase
{
public:
	std::string GetName() const override { return "CompareActorValue"; }
	std::string GetDescription() const override { return "Compare an actor value against a threshold"; }
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
	std::string GetDescription() const override { return "Compare current magazine ammo count of equipped weapon"; }
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
	std::string GetDescription() const override { return "Does the equipped weapon have a specific keyword?"; }
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
	std::string GetDescription() const override { return "Is a specific weapon/item equipped?"; }
	std::string GetParameterString() const override { return form.GetDisplayString(); }
	void DrawEditWidgets(bool& a_dirty) override { form.DrawEditWidgets("Form", a_dirty); }
protected:
	bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const override;
	void InitializeImpl(const nlohmann::json& a_json) override;
	void SerializeImpl(nlohmann::json& a_json) const override;
private:
	FormComponent form;
};

void RegisterAllConditions();
std::unique_ptr<ICondition> CreateConditionFromJson(const nlohmann::json& a_json);
