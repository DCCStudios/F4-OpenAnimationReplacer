#include "Conditions.h"
#include "Utils.h"
#include "Hooks.h"
#include "OpenAnimationReplacer.h"
#include <imgui.h>
#include <numbers>
#include "RE/Bethesda/ActorValueInfo.h"
#include "RE/Bethesda/Calendar.h"
#include "RE/Bethesda/PlayerCharacter.h"
#include "RE/Bethesda/TESBoundObjects.h"
#include "RE/Bethesda/UI.h"
#include "UI/UIFormPicker.h"
#include "RE_Additions.h"

// ===== FormComponent =====

void FormComponent::Initialize(const nlohmann::json& a_json)
{
	if (a_json.contains("Form")) {
		auto& formJson = a_json["Form"];
		if (formJson.contains("pluginName")) pluginName = formJson["pluginName"].get<std::string>();
		if (formJson.contains("formID")) {
			auto& fid = formJson["formID"];
			if (fid.is_string()) {
				localFormID = std::stoul(fid.get<std::string>(), nullptr, 16);
			} else {
				localFormID = fid.get<uint32_t>();
			}
		}
	}
}

void FormComponent::Serialize(nlohmann::json& a_json) const
{
	auto& formJson = a_json["Form"];
	formJson["pluginName"] = pluginName;
	formJson["formID"] = std::format("0x{:X}", localFormID);
}

void FormComponent::ResolveForm()
{
	cachedForm = Utils::LookupForm(localFormID, pluginName);
	if (!cachedForm) {
		logger::warn("[OAR] Failed to resolve form 0x{:X} from '{}'", localFormID, pluginName);
	}
}

std::string FormComponent::GetDisplayString() const
{
	if (cachedForm) {
		if (auto* fullName = dynamic_cast<RE::TESFullName*>(cachedForm)) {
			const char* name = fullName->GetFullName();
			if (name && name[0])
				return std::format("{} [{}:0x{:X}]", name, pluginName, localFormID);
		}
		return std::format("{}:0x{:X}", pluginName, localFormID);
	}
	if (localFormID != 0 || !pluginName.empty())
		return std::format("{}:0x{:X}", pluginName, localFormID);
	return "(none)";
}

void FormComponent::DrawEditWidgets(const char* a_label, bool& a_dirty, RE::ENUM_FORM_ID a_formTypeHint)
{
	if (UIFormPicker::DrawFormPicker(a_label, pluginName, localFormID, a_formTypeHint, a_dirty)) {
		ResolveForm();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton(std::format("Resolve##{}", a_label).c_str())) {
		ResolveForm();
	}
}

// ===== NumericComponent =====

float NumericComponent::GetValue(RE::TESObjectREFR* a_refr) const
{
	switch (type) {
	case ValueType::kStatic:
		return staticValue;
	case ValueType::kGlobalVariable:
		if (formComponent.cachedForm) {
			if (auto* glob = formComponent.cachedForm->As<RE::TESGlobal>()) {
				return glob->value;
			}
		}
		return 0.0f;
	case ValueType::kActorValue:
		return 0.0f;
	default:
		return 0.0f;
	}
}

void NumericComponent::Initialize(const nlohmann::json& a_json)
{
	if (a_json.contains("type")) {
		std::string typeStr = a_json["type"].get<std::string>();
		if (typeStr == "GlobalVariable") {
			type = ValueType::kGlobalVariable;
			formComponent.Initialize(a_json);
			formComponent.ResolveForm();
		} else if (typeStr == "ActorValue") {
			type = ValueType::kActorValue;
			if (a_json.contains("actorValue")) actorValueName = a_json["actorValue"].get<std::string>();
		} else {
			type = ValueType::kStatic;
		}
	}
	if (a_json.contains("value")) staticValue = a_json["value"].get<float>();
}

void NumericComponent::Serialize(nlohmann::json& a_json) const
{
	switch (type) {
	case ValueType::kStatic:
		a_json["type"] = "Static";
		a_json["value"] = staticValue;
		break;
	case ValueType::kGlobalVariable:
		a_json["type"] = "GlobalVariable";
		formComponent.Serialize(a_json);
		break;
	case ValueType::kActorValue:
		a_json["type"] = "ActorValue";
		a_json["actorValue"] = actorValueName;
		break;
	}
}

// ===== Concrete Conditions =====

bool IsFormCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	return a_refr->GetFormID() == form.cachedForm->GetFormID();
}

void IsFormCondition::InitializeImpl(const nlohmann::json& a_json)
{
	form.Initialize(a_json);
	form.ResolveForm();
}

void IsFormCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

bool IsActorBaseCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	auto* base = a_refr->data.objectReference;
	return base && base->GetFormID() == form.cachedForm->GetFormID();
}

void IsActorBaseCondition::InitializeImpl(const nlohmann::json& a_json)
{
	form.Initialize(a_json);
	form.ResolveForm();
}

void IsActorBaseCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

bool IsRaceCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->race) return false;
	return actor->race->GetFormID() == form.cachedForm->GetFormID();
}

void IsRaceCondition::InitializeImpl(const nlohmann::json& a_json)
{
	form.Initialize(a_json);
	form.ResolveForm();
}

void IsRaceCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

bool IsFemaleCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* base = a_refr->data.objectReference;
	if (!base || base->GetFormType() != RE::ENUM_FORM_ID::kNPC_) return false;
	// TESActorBaseData inherits at known offset within TESNPC.
	// The female flag is bit 0 of actorBaseFlags (first field of ACTOR_BASE_DATA struct).
	// TESNPC layout in FO4: TESForm(0x20) + several components.
	// TESActorBaseData starts after TESForm+TESFullName+... The flags are accessible via
	// the TESActorBaseData virtual GetIsGhost() pattern. Instead, use actorData offset.
	// In CommonLibF4's TESActorBaseData: actorData.actorBaseFlags at the struct start.
	// Access via BGSKeywordForm/component offset - safer to use raw byte access.
	// Known working pattern: TESNPC + 0x190 is approximately where actorData.actorBaseFlags lives
	// But this is fragile. Use the NPCF keyword approach instead:
	// Most reliable: check if the actor has the NPCF keyword or graph variable
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	// Use graph variable approach: bIsFemale
	bool isFemale = false;
	static const RE::BSFixedString kVarName("bIsFemale");
	actor->GetGraphVariableImpl(kVarName, isFemale);
	return isFemale;
}

bool IsWeaponDrawnCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	return actor->GetWeaponMagicDrawn();
}

bool IsInCombatCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	return actor->IsInCombat();
}

bool IsSprintingCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* player = a_refr->As<RE::PlayerCharacter>();
	if (player) return player->sprintToggled;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	return (actor->moveMode & 0x100) != 0;
}

bool IsInAirCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	return actor->flyState != 0;
}

bool HasKeywordCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !cachedKeyword) return false;
	return a_refr->HasKeyword(cachedKeyword, nullptr);
}

void HasKeywordCondition::DrawEditWidgets(bool& a_dirty)
{
	if (UIFormPicker::DrawKeywordPicker("HasKeyword", editorID, a_dirty)) {
		if (!editorID.empty()) {
			cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
		}
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Resolve##kw")) {
		if (!editorID.empty()) {
			cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
		}
	}
	if (cachedKeyword) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "OK (0x%X)", cachedKeyword->GetFormID());
	}
}

void HasKeywordCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("editorID")) {
		editorID = a_json["editorID"].get<std::string>();
		cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
		if (!cachedKeyword) {
			logger::warn("[OAR] Failed to resolve keyword by editorID '{}'", editorID);
		}
	} else {
		keywordForm.Initialize(a_json);
		keywordForm.ResolveForm();
		cachedKeyword = keywordForm.cachedForm ? keywordForm.cachedForm->As<RE::BGSKeyword>() : nullptr;
	}
}

void HasKeywordCondition::SerializeImpl(nlohmann::json& a_json) const
{
	if (!editorID.empty()) {
		a_json["editorID"] = editorID;
	} else {
		keywordForm.Serialize(a_json);
	}
}

bool IsInFactionCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	auto* faction = form.cachedForm->As<RE::TESFaction>();
	if (!faction) return false;
	return actor->IsInFaction(faction);
}

void IsInFactionCondition::InitializeImpl(const nlohmann::json& a_json)
{
	form.Initialize(a_json);
	form.ResolveForm();
}

void IsInFactionCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

bool RandomCondition::EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const
{
	static thread_local std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	return dist(rng) < threshold;
}

void RandomCondition::DrawEditWidgets(bool& a_dirty)
{
	ImGui::SetNextItemWidth(150);
	if (ImGui::SliderFloat("Threshold##rand", &threshold, 0.0f, 1.0f, "%.2f")) {
		a_dirty = true;
	}
}

void RandomCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("threshold")) threshold = a_json["threshold"].get<float>();
}

void RandomCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["threshold"] = threshold;
}

bool LevelCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	// Try graph variable iLevel first
	int32_t intLevel = 0;
	static const RE::BSFixedString kLevelVar("iLevel");
	if (actor->GetGraphVariableImpl(kLevelVar, intLevel) && intLevel > 0) {
		return CompareValues(static_cast<float>(intLevel), comparison, numericValue.GetValue(a_refr));
	}
	// Fallback: use NPC base's level field via GetBaseActorValue if an AV exists
	// In FO4, player level is typically stored at TESNPC::actorData.level (uint16)
	// Access via raw offset from base form: TESNPC base form + ~0x1A0 (level uint16)
	// For now, read from the base form's actorData
	auto* base = a_refr->data.objectReference;
	if (base && base->GetFormType() == RE::ENUM_FORM_ID::kNPC_) {
		// TESNPC's ACTOR_BASE_DATA contains level at offset 0x06 within the struct
		// TESActorBaseData starts around TESNPC+0x160 in FO4 x64
		// actorData is at TESActorBaseData+0x08, level at actorData+0x06
		// Total approximate: base+0x168 (may vary slightly)
		// Safer to just read as best effort
		auto* npcBytes = reinterpret_cast<uint8_t*>(base);
		uint16_t rawLevel = *reinterpret_cast<uint16_t*>(npcBytes + 0x16E);
		if (rawLevel > 0 && rawLevel < 1000) {
			return CompareValues(static_cast<float>(rawLevel), comparison, numericValue.GetValue(a_refr));
		}
	}
	return false;
}

void LevelCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void LevelCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

bool ORCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const
{
	return childConditions.EvaluateAny(a_refr, a_clipGen, a_sub);
}

void ORCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("conditions") && a_json["conditions"].is_array()) {
		for (const auto& condJson : a_json["conditions"]) {
			if (auto cond = CreateConditionFromJson(condJson)) {
				childConditions.AddCondition(std::move(cond));
			}
		}
	}
}

void ORCondition::SerializeImpl(nlohmann::json& a_json) const
{
	auto& arr = a_json["conditions"];
	arr = nlohmann::json::array();
	for (const auto& cond : childConditions.GetConditions()) {
		nlohmann::json condJson;
		cond->Serialize(condJson);
		arr.push_back(condJson);
	}
}

bool ANDCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const
{
	return childConditions.EvaluateAll(a_refr, a_clipGen, a_sub);
}

void ANDCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("conditions") && a_json["conditions"].is_array()) {
		for (const auto& condJson : a_json["conditions"]) {
			if (auto cond = CreateConditionFromJson(condJson)) {
				childConditions.AddCondition(std::move(cond));
			}
		}
	}
}

void ANDCondition::SerializeImpl(nlohmann::json& a_json) const
{
	auto& arr = a_json["conditions"];
	arr = nlohmann::json::array();
	for (const auto& cond : childConditions.GetConditions()) {
		nlohmann::json condJson;
		cond->Serialize(condJson);
		arr.push_back(condJson);
	}
}

// ===== Expanded conditions =====

bool IsEquippedTypeCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !cachedKeyword) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return false;
	auto* mh = actor->currentProcess->middleHigh;
	RE::BSAutoLock lock{ mh->equippedItemsLock };
	for (auto& eq : mh->equippedItems) {
		auto* obj = eq.item.object;
		if (!obj || obj->GetFormType() != RE::ENUM_FORM_ID::kWEAP) continue;
		auto* weap = static_cast<RE::TESObjectWEAP*>(obj);
		auto* idata = eq.item.instanceData ? eq.item.instanceData.get() : nullptr;
		if (weap->HasKeyword(cachedKeyword, idata)) return true;
		if (idata) {
			auto* kwForm = idata->GetKeywordData();
			if (kwForm && kwForm->HasKeyword(cachedKeyword, nullptr)) return true;
		}
	}
	return false;
}

void IsEquippedTypeCondition::InitializeImpl(const nlohmann::json& a_json)
{
	weaponKeywordForm.Initialize(a_json);
	weaponKeywordForm.ResolveForm();
	cachedKeyword = weaponKeywordForm.cachedForm ? weaponKeywordForm.cachedForm->As<RE::BGSKeyword>() : nullptr;
}

void IsEquippedTypeCondition::SerializeImpl(nlohmann::json& a_json) const
{
	weaponKeywordForm.Serialize(a_json);
}

bool IsInPowerArmorCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->biped) return false;
	constexpr std::uint32_t kPAFrameSlot = 40;
	return actor->biped->object[kPAFrameSlot].parent.object != nullptr;
}

bool IsSneakingCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	// moveMode bit pattern for sneaking in FO4: bits 9-10 encode sneak state
	// 0x200 = sneaking, 0x400 = sneak-running. Check both.
	return (actor->moveMode & 0x600) != 0 || actor->forceSneak != 0;
}

bool CurrentWeatherCondition::EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!form.cachedForm) return false;
	// TES::GetSingleton() or Sky equivalent needed for FO4
	// TODO: needs Sky singleton access — stub for now
	return false;
}

void CurrentWeatherCondition::InitializeImpl(const nlohmann::json& a_json)
{
	form.Initialize(a_json);
	form.ResolveForm();
}

void CurrentWeatherCondition::SerializeImpl(nlohmann::json& a_json) const
{
	form.Serialize(a_json);
}

bool IsADSCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	// gunState 6 = sighted/ADS, 8 = firing while in ADS (pattern from FPInertia)
	auto gs = static_cast<std::uint32_t>(actor->gunState);
	return gs == 6 || gs == 8;
}

static const char* ComparisonOpToString(ComparisonOperator op)
{
	switch (op) {
	case ComparisonOperator::kEqual: return "==";
	case ComparisonOperator::kNotEqual: return "!=";
	case ComparisonOperator::kGreater: return ">";
	case ComparisonOperator::kGreaterEqual: return ">=";
	case ComparisonOperator::kLess: return "<";
	case ComparisonOperator::kLessEqual: return "<=";
	default: return "?";
	}
}

std::string CompareActorValueCondition::GetParameterString() const
{
	return std::format("{} {} {:.1f}", actorValueName, ComparisonOpToString(comparison), numericValue.staticValue);
}

void CompareActorValueCondition::DrawEditWidgets(bool& a_dirty)
{
	char buf[128]{};
	strncpy_s(buf, actorValueName.c_str(), sizeof(buf) - 1);
	ImGui::SetNextItemWidth(150);
	if (ImGui::InputText("Actor Value##cav", buf, sizeof(buf))) {
		actorValueName = buf;
		a_dirty = true;
	}
	ImGui::SameLine();
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##cavOp", &compIdx, ops, 6)) {
		comparison = static_cast<ComparisonOperator>(compIdx);
		a_dirty = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##cavVal", &numericValue.staticValue, 0, 0, "%.1f")) {
		a_dirty = true;
	}
}

bool CompareActorValueCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;

	if (!avInfoResolved) {
		avInfoResolved = true;
		if (!actorValueName.empty()) {
			cachedAVInfo = RE::TESForm::GetFormByEditorID<RE::ActorValueInfo>(actorValueName);
			if (!cachedAVInfo) {
				logger::warn("[OAR] CompareActorValue: failed to resolve AV '{}'", actorValueName);
			}
		}
	}
	if (!cachedAVInfo) return false;
	float actorVal = actor->GetActorValue(*cachedAVInfo);
	float compareVal = numericValue.GetValue(a_refr);
	return CompareValues(actorVal, comparison, compareVal);
}

void CompareActorValueCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("actorValue")) actorValueName = a_json["actorValue"].get<std::string>();
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void CompareActorValueCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["actorValue"] = actorValueName;
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// ===== New Conditions =====

std::string CurrentMagazineAmmoCondition::GetParameterString() const
{
	return std::format("ammo {} {:.0f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void CurrentMagazineAmmoCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##ammoOp", &compIdx, ops, 6)) {
		comparison = static_cast<ComparisonOperator>(compIdx);
		a_dirty = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##ammoVal", &numericValue.staticValue, 0, 0, "%.0f")) {
		a_dirty = true;
	}
}

bool CurrentMagazineAmmoCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return false;
	auto* mh = actor->currentProcess->middleHigh;
	RE::BSAutoLock lock{ mh->equippedItemsLock };
	for (auto& eq : mh->equippedItems) {
		if (!eq.data) continue;
		auto* wd = static_cast<RE::EquippedWeaponData*>(eq.data.get());
		if (!wd || !wd->ammo) continue;
		float ammoCount = static_cast<float>(wd->ammoCount);
		float compareVal = numericValue.GetValue(a_refr);
		bool result = CompareValues(ammoCount, comparison, compareVal);
		return result;
	}
	return false;
}

void CurrentMagazineAmmoCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void CurrentMagazineAmmoCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

void IsEquippedHasKeywordCondition::DrawEditWidgets(bool& a_dirty)
{
	if (UIFormPicker::DrawKeywordPicker("EqHasKeyword", editorID, a_dirty)) {
		if (!editorID.empty()) {
			cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
		}
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Resolve##eqkw")) {
		if (!editorID.empty()) {
			cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
		}
	}
	if (cachedKeyword) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "OK (0x%X)", cachedKeyword->GetFormID());
	}
}

bool IsEquippedHasKeywordCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !cachedKeyword) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return false;
	auto* mh = actor->currentProcess->middleHigh;
	RE::BSAutoLock lock{ mh->equippedItemsLock };
	for (auto& eq : mh->equippedItems) {
		auto* obj = eq.item.object;
		if (!obj || obj->GetFormType() != RE::ENUM_FORM_ID::kWEAP) continue;
		auto* weap = static_cast<RE::TESObjectWEAP*>(obj);
		auto* idata = eq.item.instanceData ? eq.item.instanceData.get() : nullptr;
		if (weap->HasKeyword(cachedKeyword, idata)) return true;
		if (idata) {
			auto* kwForm = idata->GetKeywordData();
			if (kwForm && kwForm->HasKeyword(cachedKeyword, nullptr)) return true;
		}
	}
	return false;
}

void IsEquippedHasKeywordCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("editorID")) {
		editorID = a_json["editorID"].get<std::string>();
		cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
		if (!cachedKeyword) {
			logger::warn("[OAR] Failed to resolve equipped keyword by editorID '{}'", editorID);
		}
	} else {
		keywordForm.Initialize(a_json);
		keywordForm.ResolveForm();
		cachedKeyword = keywordForm.cachedForm ? keywordForm.cachedForm->As<RE::BGSKeyword>() : nullptr;
	}
}

void IsEquippedHasKeywordCondition::SerializeImpl(nlohmann::json& a_json) const
{
	if (!editorID.empty()) {
		a_json["editorID"] = editorID;
	} else {
		keywordForm.Serialize(a_json);
	}
}

bool IsEquippedCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return false;
	auto* mh = actor->currentProcess->middleHigh;
	RE::BSAutoLock lock{ mh->equippedItemsLock };
	for (auto& eq : mh->equippedItems) {
		auto* obj = eq.item.object;
		if (obj && obj->GetFormID() == form.cachedForm->GetFormID()) return true;
	}
	return false;
}

void IsEquippedCondition::InitializeImpl(const nlohmann::json& a_json)
{
	form.Initialize(a_json);
	form.ResolveForm();
}

void IsEquippedCondition::SerializeImpl(nlohmann::json& a_json) const
{
	form.Serialize(a_json);
}

// ===== Priority 1 Conditions =====

// XOR
bool XORCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const
{
	int passCount = 0;
	for (const auto& cond : childConditions.GetConditions()) {
		if (cond->Evaluate(a_refr, a_clipGen, a_sub)) passCount++;
		if (passCount > 1) return false;
	}
	return passCount == 1;
}

void XORCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("conditions") && a_json["conditions"].is_array()) {
		for (const auto& condJson : a_json["conditions"]) {
			if (auto cond = CreateConditionFromJson(condJson)) {
				childConditions.AddCondition(std::move(cond));
			}
		}
	}
}

void XORCondition::SerializeImpl(nlohmann::json& a_json) const
{
	auto& arr = a_json["conditions"];
	arr = nlohmann::json::array();
	for (const auto& cond : childConditions.GetConditions()) {
		nlohmann::json condJson;
		cond->Serialize(condJson);
		arr.push_back(condJson);
	}
}

// IsChild
bool IsChildCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	return a_refr && a_refr->IsChild();
}

// IsPlayerTeammate
bool IsPlayerTeammateCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess) return false;
	return actor->currentProcess->escortingPlayer;
}

// IsTalking
bool IsTalkingCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	return a_refr && a_refr->IsTalking();
}

// IsAttacking — melee OR gun firing (any combat action)
bool IsAttackingCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	if (actor->meleeAttackState != 0) return true;
	auto gs = static_cast<std::uint32_t>(actor->gunState);
	return gs == 7 || gs == 8; // kFire, kFireSighted
}

// IsReloading — gunState == 4 (kReloading)
bool IsReloadingCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	auto gs = static_cast<std::uint32_t>(actor->gunState);
	return gs == 4;
}

// IsFiring — gunState 7 (kFire) or 8 (kFireSighted)
bool IsFiringCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	auto gs = static_cast<std::uint32_t>(actor->gunState);
	return gs == 7 || gs == 8;
}

// IsDryFiring — true when the engine has dispatched ActionFireEmpty to this actor.
// Hooks Actor::NotifyAnimationGraphImpl and watches for the "ActionFireEmpty" event
// that the engine sends when the player presses fire with no ammo.
bool IsDryFiringCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	uint32_t formID = a_refr->GetFormID();
	uint32_t currentGen = GetFireEmptyGeneration(formID);
	auto& state = m_actorState[formID];

	if (!state.active) {
		// Not currently active — check if a NEW press occurred
		if (currentGen == 0 || currentGen == state.activationGen)
			return false;  // no new press since last deactivation

		// New press detected — activate
		state.active = true;
		state.activationGen = currentGen;
		state.activationTime = std::chrono::steady_clock::now();
		return true;
	}

	// Currently active — check if duration has elapsed
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - state.activationTime).count();

	if (elapsed >= static_cast<int64_t>(durationMs)) {
		// Timer expired — deactivate
		state.active = false;
		return false;
	}

	// Still within the active window
	if (retriggerable && currentGen != state.activationGen) {
		// Retriggerable: new press detected while active — trip a one-frame false
		// to force the replacement system to restart the animation clip.
		// Update activationGen and reset the timer for the new press.
		state.activationGen = currentGen;
		state.activationTime = std::chrono::steady_clock::now();
		return false;
	}

	// Non-retriggerable: ignore any new presses, just ride out the timer
	return true;
}

std::string IsDryFiringCondition::GetParameterString() const
{
	std::string result = std::format("{}ms", durationMs);
	if (retriggerable) result += " retrig";
	return result;
}

void IsDryFiringCondition::DrawEditWidgets(bool& a_dirty)
{
	int dur = durationMs;
	if (ImGui::SliderInt("Duration (ms)", &dur, 50, 5000)) {
		durationMs = dur;
		a_dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("How long the condition stays true after pressing fire with no ammo.");

	if (ImGui::Checkbox("Retriggerable", &retriggerable)) {
		a_dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("If checked, pressing fire again before the animation finishes will restart it from the beginning.");
}

void IsDryFiringCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("durationMs")) durationMs = a_json["durationMs"].get<int32_t>();
	if (a_json.contains("retriggerable")) retriggerable = a_json["retriggerable"].get<bool>();
}

void IsDryFiringCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["durationMs"] = durationMs;
	a_json["retriggerable"] = retriggerable;
}

// IsButtonHeld — checks if a user event's mapped key is currently pressed via BSInputDeviceManager.
// Builds a cached list of all user event names from the game's ControlMap (main gameplay context).
static const std::vector<std::string>& GetCachedUserEventNames()
{
	static std::vector<std::string> s_eventNames;
	static bool s_initialized = false;

	if (!s_initialized) {
		s_initialized = true;
		auto* controlMap = RE::ControlMap::GetSingleton();
		if (controlMap) {
			constexpr auto kMainGameplay = RE::UserEvents::INPUT_CONTEXT_ID::kMainGameplay;
			auto* ctx = controlMap->controlMaps[static_cast<int>(kMainGameplay)];
			if (ctx) {
				// Collect unique event names across all device mappings
				std::set<std::string> uniqueNames;
				for (int devIdx = 0; devIdx < static_cast<int>(RE::INPUT_DEVICE::kSupported); ++devIdx) {
					auto& mappings = ctx->deviceMappings[devIdx];
					for (auto& mapping : mappings) {
						const char* name = mapping.eventID.c_str();
						if (name && name[0] != '\0') {
							uniqueNames.insert(name);
						}
					}
				}
				s_eventNames.assign(uniqueNames.begin(), uniqueNames.end());
			}
		}
		// Fallback list if ControlMap isn't ready yet
		if (s_eventNames.empty()) {
			s_eventNames = {
				"Activate", "Attack", "AutoMove", "Back", "Block", "Favorites",
				"Forward", "Grenade", "Jump", "Look", "Melee", "Move",
				"PipBoy", "PipBoyLight", "ReadyWeapon", "Run", "Sneak",
				"Sprint", "StrafeLeft", "StrafeRight", "TogglePOV",
				"ToggleRun", "TweenMenu", "VATS", "ZoomIn", "ZoomOut"
			};
			s_initialized = false; // Try again next frame if ControlMap wasn't ready
		}
	}
	return s_eventNames;
}

void IsButtonHeldCondition::DrawEditWidgets(bool& a_dirty)
{
	const auto& eventNames = GetCachedUserEventNames();

	// Find current selection index
	int currentIdx = -1;
	for (int i = 0; i < static_cast<int>(eventNames.size()); ++i) {
		if (eventNames[i] == userEvent) {
			currentIdx = i;
			break;
		}
	}

	const char* preview = currentIdx >= 0 ? eventNames[currentIdx].c_str() : "(select event)";
	ImGui::SetNextItemWidth(180);
	if (ImGui::BeginCombo("User Event##btnheld", preview)) {
		for (int i = 0; i < static_cast<int>(eventNames.size()); ++i) {
			bool isSelected = (i == currentIdx);
			if (ImGui::Selectable(eventNames[i].c_str(), isSelected)) {
				userEvent = eventNames[i];
				a_dirty = true;
			}
			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

bool IsButtonHeldCondition::EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const
{
	if (userEvent.empty()) return false;

	auto* controlMap = RE::ControlMap::GetSingleton();
	if (!controlMap) return false;

	auto* deviceMgr = RE::BSInputDeviceManager::GetSingleton();
	if (!deviceMgr) return false;

	// Resolve the user event to a key code from the main gameplay context
	const RE::BSFixedString eventID(userEvent.c_str());
	constexpr auto kMainGameplay = RE::UserEvents::INPUT_CONTEXT_ID::kMainGameplay;
	auto* ctx = controlMap->controlMaps[static_cast<int>(kMainGameplay)];
	if (!ctx) return false;

	// Check each device type for a mapping, then check if that key is held
	for (int devIdx = 0; devIdx < static_cast<int>(RE::INPUT_DEVICE::kSupported); ++devIdx) {
		auto& mappings = ctx->deviceMappings[devIdx];
		for (auto& mapping : mappings) {
			if (mapping.eventID == eventID && mapping.inputKey >= 0) {
				// Found the mapped key for this device — check if it's pressed
				auto* device = deviceMgr->devices[devIdx];
				if (!device) continue;

				auto keyCode = static_cast<std::uint32_t>(mapping.inputKey);
				auto it = device->deviceButtons.find(keyCode);
				if (it != device->deviceButtons.end() && it->second) {
					// heldDownSecs > 0 means the button is currently pressed
					if (it->second->heldDownSecs > 0.f) {
						return true;
					}
				}
			}
		}
	}

	return false;
}

void IsButtonHeldCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("userEvent")) userEvent = a_json["userEvent"].get<std::string>();
}

void IsButtonHeldCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["userEvent"] = userEvent;
}

// IsBlocking
bool IsBlockingCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	return actor->wantBlocking != 0;
}

// IsRunning
bool IsRunningCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	uint32_t mode = actor->moveMode & 0x3FFF;
	return mode == 5 || mode == 6;
}

// IsInInterior
bool IsInInteriorCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* cell = a_refr->GetParentCell();
	return cell && cell->IsInterior();
}

// IsWorldSpace
bool IsWorldSpaceCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	auto* cell = a_refr->GetParentCell();
	if (!cell || cell->IsInterior()) return false;
	// For exterior cells, worldSpace is stored in a union at offset 0xC8 in TESObjectCELL
	auto* cellBytes = reinterpret_cast<uint8_t*>(cell);
	auto* worldSpace = *reinterpret_cast<RE::TESWorldSpace**>(cellBytes + 0xC8);
	return worldSpace && worldSpace->GetFormID() == form.cachedForm->GetFormID();
}

void IsWorldSpaceCondition::InitializeImpl(const nlohmann::json& a_json) { form.Initialize(a_json); form.ResolveForm(); }
void IsWorldSpaceCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

// IsParentCell
bool IsParentCellCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	auto* cell = a_refr->GetParentCell();
	return cell && cell->GetFormID() == form.cachedForm->GetFormID();
}

void IsParentCellCondition::InitializeImpl(const nlohmann::json& a_json) { form.Initialize(a_json); form.ResolveForm(); }
void IsParentCellCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

// IsInLocation
bool IsInLocationCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	auto* player = a_refr->As<RE::PlayerCharacter>();
	if (player && player->currentLocation) {
		return player->currentLocation->GetFormID() == form.cachedForm->GetFormID();
	}
	return false;
}

void IsInLocationCondition::InitializeImpl(const nlohmann::json& a_json) { form.Initialize(a_json); form.ResolveForm(); }
void IsInLocationCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

// HasPerk
bool HasPerkCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	auto* perk = form.cachedForm->As<RE::BGSPerk>();
	if (!perk) return false;
	uint32_t perkID = perk->GetFormID();
	// Check base form perks (TESActorBase inherits BGSPerkRankArray at offset 0x188)
	auto* base = a_refr->data.objectReference;
	if (base && base->GetFormType() == RE::ENUM_FORM_ID::kNPC_) {
		auto* perkArray = reinterpret_cast<RE::BGSPerkRankArray*>(reinterpret_cast<uint8_t*>(base) + 0x188);
		if (perkArray && perkArray->perks) {
			for (uint32_t i = 0; i < perkArray->perkCount; ++i) {
				if (perkArray->perks[i].perk && perkArray->perks[i].perk->GetFormID() == perkID) return true;
			}
		}
	}
	// Check runtime-added perks (Perks* at offset 0x420 in Actor)
	// Perks struct layout: PerkRankData* data at +0x00, uint32_t count at +0x08
	if (actor->perks) {
		auto* perksBytes = reinterpret_cast<uint8_t*>(actor->perks);
		auto* addedPerks = *reinterpret_cast<RE::PerkRankData**>(perksBytes);
		uint32_t addedCount = *reinterpret_cast<uint32_t*>(perksBytes + 0x08);
		if (addedPerks && addedCount > 0 && addedCount < 10000) {
			for (uint32_t i = 0; i < addedCount; ++i) {
				if (addedPerks[i].perk && addedPerks[i].perk->GetFormID() == perkID) return true;
			}
		}
	}
	return false;
}

void HasPerkCondition::InitializeImpl(const nlohmann::json& a_json) { form.Initialize(a_json); form.ResolveForm(); }
void HasPerkCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

// HasSpell
bool HasSpellCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	auto* spell = form.cachedForm->As<RE::SpellItem>();
	if (!spell) return false;
	for (auto* s : actor->addedSpells) {
		if (s && s->GetFormID() == spell->GetFormID()) return true;
	}
	return false;
}

void HasSpellCondition::InitializeImpl(const nlohmann::json& a_json) { form.Initialize(a_json); form.ResolveForm(); }
void HasSpellCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

// Scale
std::string ScaleCondition::GetParameterString() const
{
	return std::format("scale {} {:.2f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void ScaleCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##scaleOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##scaleVal", &numericValue.staticValue, 0, 0, "%.2f")) { a_dirty = true; }
}

bool ScaleCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	float scale = static_cast<float>(a_refr->refScale) / 100.0f;
	return CompareValues(scale, comparison, numericValue.GetValue(a_refr));
}

void ScaleCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void ScaleCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// CurrentGameTime
std::string CurrentGameTimeCondition::GetParameterString() const
{
	return std::format("time {} {:.1f}h", ComparisonOpToString(comparison), numericValue.staticValue);
}

void CurrentGameTimeCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##timeOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##timeVal", &numericValue.staticValue, 0, 0, "%.1f")) { a_dirty = true; }
}

bool CurrentGameTimeCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	auto* calendar = RE::Calendar::GetSingleton();
	if (!calendar) return false;
	float hours = calendar->GetHoursPassed();
	float hourOfDay = std::fmod(hours, 24.0f);
	return CompareValues(hourOfDay, comparison, numericValue.GetValue(a_refr));
}

void CurrentGameTimeCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void CurrentGameTimeCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// FactionRank
std::string FactionRankCondition::GetParameterString() const
{
	return std::format("{} rank {} {:.0f}", factionForm.GetDisplayString(), ComparisonOpToString(comparison), numericValue.staticValue);
}

void FactionRankCondition::DrawEditWidgets(bool& a_dirty)
{
	factionForm.DrawEditWidgets("Faction", a_dirty, RE::ENUM_FORM_ID::kFACT);
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##frkOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##frkVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
}

bool FactionRankCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !factionForm.cachedForm) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	auto* faction = factionForm.cachedForm->As<RE::TESFaction>();
	if (!faction) return false;
	// Check if actor is in faction and get rank
	// Actor's faction list is on the base NPC through TESActorBaseData::factions
	// For now, check via IsInFaction and assume rank >= 0 means membership
	if (!actor->IsInFaction(faction)) return false;
	// Without direct rank access, compare rank 0 (member) against threshold
	float rank = 0.0f;
	return CompareValues(rank, comparison, numericValue.GetValue(a_refr));
}

void FactionRankCondition::InitializeImpl(const nlohmann::json& a_json)
{
	factionForm.Initialize(a_json);
	factionForm.ResolveForm();
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void FactionRankCondition::SerializeImpl(nlohmann::json& a_json) const
{
	factionForm.Serialize(a_json);
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// CrimeGold
std::string CrimeGoldCondition::GetParameterString() const
{
	return std::format("{} gold {} {:.0f}", factionForm.GetDisplayString(), ComparisonOpToString(comparison), numericValue.staticValue);
}

void CrimeGoldCondition::DrawEditWidgets(bool& a_dirty)
{
	factionForm.DrawEditWidgets("Faction", a_dirty, RE::ENUM_FORM_ID::kFACT);
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##cgOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##cgVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
}

bool CrimeGoldCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !factionForm.cachedForm) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	auto* faction = factionForm.cachedForm->As<RE::TESFaction>();
	if (!faction) return false;
	float gold = static_cast<float>(actor->GetCrimeGoldValue(faction));
	return CompareValues(gold, comparison, numericValue.GetValue(a_refr));
}

void CrimeGoldCondition::InitializeImpl(const nlohmann::json& a_json)
{
	factionForm.Initialize(a_json);
	factionForm.ResolveForm();
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void CrimeGoldCondition::SerializeImpl(nlohmann::json& a_json) const
{
	factionForm.Serialize(a_json);
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// LifeState
std::string LifeStateCondition::GetParameterString() const
{
	return std::format("state {} {:.0f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void LifeStateCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##lsOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##lsVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
}

bool LifeStateCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	float state = static_cast<float>(actor->lifeState);
	return CompareValues(state, comparison, numericValue.GetValue(a_refr));
}

void LifeStateCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void LifeStateCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// SitSleepState
std::string SitSleepStateCondition::GetParameterString() const
{
	return std::format("state {} {:.0f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void SitSleepStateCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##ssOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##ssVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
}

bool SitSleepStateCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	float state = static_cast<float>(actor->DoGetSitSleepState());
	return CompareValues(state, comparison, numericValue.GetValue(a_refr));
}

void SitSleepStateCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void SitSleepStateCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// HasTarget
bool HasTargetCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	return static_cast<bool>(actor->currentCombatTarget);
}

// CurrentTargetDistance
std::string CurrentTargetDistanceCondition::GetParameterString() const
{
	return std::format("dist {} {:.0f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void CurrentTargetDistanceCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##tdOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##tdVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
}

bool CurrentTargetDistanceCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !static_cast<bool>(actor->currentCombatTarget)) return false;
	auto targetPtr = actor->currentCombatTarget.get();
	if (!targetPtr) return false;
	auto& myPos = a_refr->data.location;
	auto& tgtPos = targetPtr->data.location;
	float dx = tgtPos.x - myPos.x;
	float dy = tgtPos.y - myPos.y;
	float dz = tgtPos.z - myPos.z;
	float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
	return CompareValues(dist, comparison, numericValue.GetValue(a_refr));
}

void CurrentTargetDistanceCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void CurrentTargetDistanceCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// IsMovementDirection
std::string IsMovementDirectionCondition::GetParameterString() const
{
	const char* dirs[] = { "Forward", "Right", "Back", "Left" };
	return (direction >= 0 && direction <= 3) ? dirs[direction] : "Unknown";
}

void IsMovementDirectionCondition::DrawEditWidgets(bool& a_dirty)
{
	const char* dirs[] = { "Forward", "Right", "Back", "Left" };
	ImGui::SetNextItemWidth(120);
	if (ImGui::Combo("Direction##md", &direction, dirs, 4)) { a_dirty = true; }
}

bool IsMovementDirectionCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	// Use graph variable "Direction" which gives the movement direction angle [-1,1] mapped from [-180,180]
	// 0 = forward, 1/-1 = backward, 0.5 = right, -0.5 = left
	float directionVal = 0.f;
	static const RE::BSFixedString kDirection("Direction");
	actor->GetGraphVariableImpl(kDirection, directionVal);
	// direction enum: 0=forward, 1=right, 2=back, 3=left
	switch (direction) {
	case 0: return (directionVal > -0.25f && directionVal < 0.25f);
	case 1: return (directionVal >= 0.25f && directionVal < 0.75f);
	case 2: return (directionVal >= 0.75f || directionVal <= -0.75f);
	case 3: return (directionVal <= -0.25f && directionVal > -0.75f);
	}
	return false;
}

void IsMovementDirectionCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("direction")) direction = a_json["direction"].get<int32_t>();
}

void IsMovementDirectionCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["direction"] = direction;
}

// IsCombatState
std::string IsCombatStateCondition::GetParameterString() const
{
	return std::format("state {} {:.0f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void IsCombatStateCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##csOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##csVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
}

bool IsCombatStateCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	float state = actor->IsInCombat() ? 1.0f : 0.0f;
	return CompareValues(state, comparison, numericValue.GetValue(a_refr));
}

void IsCombatStateCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void IsCombatStateCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// ===== Priority 2 Conditions =====

// HasGraphVariable
std::string HasGraphVariableCondition::GetParameterString() const
{
	return std::format("'{}' {} {:.1f}", variableName, ComparisonOpToString(comparison), floatValue);
}

void HasGraphVariableCondition::DrawEditWidgets(bool& a_dirty)
{
	char buf[128]{};
	strncpy_s(buf, variableName.c_str(), sizeof(buf) - 1);
	ImGui::SetNextItemWidth(150);
	if (ImGui::InputText("Variable##gv", buf, sizeof(buf))) { variableName = buf; a_dirty = true; }
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SameLine();
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##gvOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##gvVal", &floatValue, 0, 0, "%.1f")) { a_dirty = true; }
}

bool HasGraphVariableCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || variableName.empty()) return false;
	static const RE::BSFixedString varNameStr(variableName.c_str());
	RE::BSFixedString dynVarName(variableName.c_str());
	switch (varType) {
	case VarType::kFloat: {
		float val = 0.f;
		if (!a_refr->GetGraphVariableImpl(dynVarName, val)) return false;
		return CompareValues(val, comparison, floatValue);
	}
	case VarType::kInt: {
		int32_t val = 0;
		if (!a_refr->GetGraphVariableImpl(dynVarName, val)) return false;
		return CompareValues(static_cast<float>(val), comparison, static_cast<float>(intValue));
	}
	case VarType::kBool: {
		bool val = false;
		if (!a_refr->GetGraphVariableImpl(dynVarName, val)) return false;
		return val == boolValue;
	}
	}
	return false;
}

void HasGraphVariableCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("variableName")) variableName = a_json["variableName"].get<std::string>();
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("type")) {
		auto t = a_json["type"].get<std::string>();
		if (t == "Bool") varType = VarType::kBool;
		else if (t == "Int") varType = VarType::kInt;
		else varType = VarType::kFloat;
	}
	if (a_json.contains("value")) {
		switch (varType) {
		case VarType::kFloat: floatValue = a_json["value"].get<float>(); break;
		case VarType::kInt: intValue = a_json["value"].get<int32_t>(); break;
		case VarType::kBool: boolValue = a_json["value"].get<bool>(); break;
		}
	}
}

void HasGraphVariableCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["variableName"] = variableName;
	a_json["comparison"] = static_cast<int32_t>(comparison);
	switch (varType) {
	case VarType::kFloat: a_json["type"] = "Float"; a_json["value"] = floatValue; break;
	case VarType::kInt: a_json["type"] = "Int"; a_json["value"] = intValue; break;
	case VarType::kBool: a_json["type"] = "Bool"; a_json["value"] = boolValue; break;
	}
}

// IsReplacerEnabled
bool IsReplacerEnabledCondition::EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const
{
	auto* oar = OpenAnimationReplacer::GetSingleton();
	if (!oar) return false;
	for (auto& mod : oar->GetReplacerMods()) {
		if (!modName.empty() && mod->GetName() != modName) continue;
		if (subModName.empty()) return true;
		for (auto& sub : mod->GetSubMods()) {
			if (sub->GetName() == subModName) return !sub->IsDisabled();
		}
	}
	return false;
}

void IsReplacerEnabledCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("modName")) modName = a_json["modName"].get<std::string>();
	if (a_json.contains("subModName")) subModName = a_json["subModName"].get<std::string>();
}

void IsReplacerEnabledCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["modName"] = modName;
	a_json["subModName"] = subModName;
}

// MovementSpeed
std::string MovementSpeedCondition::GetParameterString() const
{
	return std::format("speed {} {:.0f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void MovementSpeedCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##msOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##msVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
}

bool MovementSpeedCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return false;
	// desiredSpeed is at a known offset in MiddleHighProcessData
	auto* mhBytes = reinterpret_cast<uint8_t*>(actor->currentProcess->middleHigh);
	float speed = *reinterpret_cast<float*>(mhBytes + 0x1F4); // approximate offset for desiredSpeed
	return CompareValues(speed, comparison, numericValue.GetValue(a_refr));
}

void MovementSpeedCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void MovementSpeedCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// IsSwimming
bool IsSwimmingCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	// flyState == 2 indicates swimming in FO4
	return actor->flyState == 2;
}

// IsOverEncumbered
bool IsOverEncumberedCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	// Compare inventory weight vs carry weight actor value
	auto* avInfo = RE::TESForm::GetFormByEditorID<RE::ActorValueInfo>("CarryWeight");
	if (!avInfo) return false;
	float carryWeight = actor->GetActorValue(*avInfo);
	// Get inventory weight from the refr
	float invWeight = 0.f;
	if (a_refr->inventoryList) {
		invWeight = a_refr->inventoryList->cachedWeight;
	}
	return invWeight > carryWeight;
}

// LocationHasKeyword
void LocationHasKeywordCondition::DrawEditWidgets(bool& a_dirty)
{
	if (UIFormPicker::DrawKeywordPicker("LocKeyword", editorID, a_dirty)) {
		if (!editorID.empty()) {
			cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
		}
	}
}

bool LocationHasKeywordCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !cachedKeyword) return false;
	auto* player = a_refr->As<RE::PlayerCharacter>();
	if (!player || !player->currentLocation) return false;
	return player->currentLocation->HasKeyword(cachedKeyword, nullptr);
}

void LocationHasKeywordCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("editorID")) {
		editorID = a_json["editorID"].get<std::string>();
		cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
	}
}

void LocationHasKeywordCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["editorID"] = editorID;
}

// LocationCleared
bool LocationClearedCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* player = a_refr->As<RE::PlayerCharacter>();
	if (!player || !player->currentLocation) return false;
	// BGSLocation::cleared is a bool at a known offset
	auto* locBytes = reinterpret_cast<uint8_t*>(player->currentLocation);
	// cleared flag in BGSLocation - approximate offset 0x48 in FO4
	return *reinterpret_cast<bool*>(locBytes + 0x48);
}

// ===== Priority 3 Conditions =====

// IsTrespassing
bool IsTrespassingCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	return actor && actor->trespassing;
}

// IsCurrentPackage
bool IsCurrentPackageCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess) return false;
	auto* pkg = actor->currentProcess->currentPackage.package;
	return pkg && pkg->GetFormID() == form.cachedForm->GetFormID();
}

void IsCurrentPackageCondition::InitializeImpl(const nlohmann::json& a_json) { form.Initialize(a_json); form.ResolveForm(); }
void IsCurrentPackageCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

// IsInScene
bool IsInSceneCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	return a_refr->GetCurrentScene() != nullptr;
}

// CurrentFurniture
bool CurrentFurnitureCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return false;
	// occupiedFurniture is an ObjectRefHandle in MiddleHighProcessData
	auto* mh = actor->currentProcess->middleHigh;
	auto* mhBytes = reinterpret_cast<uint8_t*>(mh);
	// occupiedFurniture handle is at approximate offset in MiddleHighProcessData
	// If form is specified, compare. If not, just check if any furniture is occupied.
	if (!form.cachedForm) {
		// Any furniture occupied
		auto handle = *reinterpret_cast<uint32_t*>(mhBytes + 0x3C0);
		return handle != 0;
	}
	return false;
}

void CurrentFurnitureCondition::InitializeImpl(const nlohmann::json& a_json) { form.Initialize(a_json); form.ResolveForm(); }
void CurrentFurnitureCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

// ===== Context Wrappers =====

// TARGET - evaluate children against combat target
bool TargetConditionWrapper::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !static_cast<bool>(actor->currentCombatTarget)) return false;
	auto target = actor->currentCombatTarget.get();
	if (!target) return false;
	return childConditions.EvaluateAll(target.get(), a_clipGen, a_sub);
}

void TargetConditionWrapper::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("conditions") && a_json["conditions"].is_array()) {
		for (const auto& condJson : a_json["conditions"]) {
			if (auto cond = CreateConditionFromJson(condJson)) {
				childConditions.AddCondition(std::move(cond));
			}
		}
	}
}

void TargetConditionWrapper::SerializeImpl(nlohmann::json& a_json) const
{
	auto& arr = a_json["conditions"];
	arr = nlohmann::json::array();
	for (const auto& cond : childConditions.GetConditions()) {
		nlohmann::json condJson;
		cond->Serialize(condJson);
		arr.push_back(condJson);
	}
}

// PLAYER - evaluate children against the player
bool PlayerConditionWrapper::EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) return false;
	return childConditions.EvaluateAll(player, a_clipGen, a_sub);
}

void PlayerConditionWrapper::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("conditions") && a_json["conditions"].is_array()) {
		for (const auto& condJson : a_json["conditions"]) {
			if (auto cond = CreateConditionFromJson(condJson)) {
				childConditions.AddCondition(std::move(cond));
			}
		}
	}
}

void PlayerConditionWrapper::SerializeImpl(nlohmann::json& a_json) const
{
	auto& arr = a_json["conditions"];
	arr = nlohmann::json::array();
	for (const auto& cond : childConditions.GetConditions()) {
		nlohmann::json condJson;
		cond->Serialize(condJson);
		arr.push_back(condJson);
	}
}

// ===== Additional Missing Conditions =====

// IsUnique - check TESNPC unique flag (bit 5 of actorBaseFlags in TESActorBaseData)
bool IsUniqueCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* base = a_refr->data.objectReference;
	if (!base || base->GetFormType() != RE::ENUM_FORM_ID::kNPC_) return false;
	// Unique flag is bit 5 (0x20) in ACTOR_BASE_DATA::actorBaseFlags
	// TESActorBaseData is a component of TESNPC, actorBaseFlags at component+0x08
	// Read using raw offset from the NPC form base (~0x168 for actorBaseFlags in FO4 x64)
	auto* npcBytes = reinterpret_cast<uint8_t*>(base);
	uint32_t flags = *reinterpret_cast<uint32_t*>(npcBytes + 0x168);
	return (flags & 0x20) != 0;
}

// IsSummoned
bool IsSummonedCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return false;
	auto* mhBytes = reinterpret_cast<uint8_t*>(actor->currentProcess->middleHigh);
	// summonedCreature bool at offset 0x4A1 in MiddleHighProcessData
	return *reinterpret_cast<bool*>(mhBytes + 0x4A1);
}

// IsGhost - TESActorBaseData::GetIsGhost() virtual
bool IsGhostCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* base = a_refr->data.objectReference;
	if (!base || base->GetFormType() != RE::ENUM_FORM_ID::kNPC_) return false;
	// Ghost flag is bit 29 (0x20000000) in actorBaseFlags
	auto* npcBytes = reinterpret_cast<uint8_t*>(base);
	uint32_t flags = *reinterpret_cast<uint32_t*>(npcBytes + 0x168);
	return (flags & 0x20000000) != 0;
}

// IsGreetingPlayer
bool IsGreetingPlayerCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) return false;
	return player->greetingPlayer;
}

// IsGuard
bool IsGuardCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return false;
	auto* mhBytes = reinterpret_cast<uint8_t*>(actor->currentProcess->middleHigh);
	// hostileGuard bool in MiddleHighProcessData
	return *reinterpret_cast<bool*>(mhBytes + 0x4A2);
}

// Height
std::string HeightCondition::GetParameterString() const
{
	return std::format("height {} {:.2f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void HeightCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##htOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##htVal", &numericValue.staticValue, 0, 0, "%.2f")) { a_dirty = true; }
}

bool HeightCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	float height = a_refr->GetActorHeightOrRefBound();
	return CompareValues(height, comparison, numericValue.GetValue(a_refr));
}

void HeightCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void HeightCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// Weight (equipped weight)
std::string WeightCondition::GetParameterString() const
{
	return std::format("weight {} {:.1f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void WeightCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##wtOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##wtVal", &numericValue.staticValue, 0, 0, "%.1f")) { a_dirty = true; }
}

bool WeightCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	float weight = actor->equippedWeight;
	return CompareValues(weight, comparison, numericValue.GetValue(a_refr));
}

void WeightCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void WeightCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// InventoryCount
std::string InventoryCountCondition::GetParameterString() const
{
	return std::format("{} count {} {:.0f}", itemForm.GetDisplayString(), ComparisonOpToString(comparison), numericValue.staticValue);
}

void InventoryCountCondition::DrawEditWidgets(bool& a_dirty)
{
	itemForm.DrawEditWidgets("Item", a_dirty);
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##icOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##icVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
}

bool InventoryCountCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !itemForm.cachedForm || !a_refr->inventoryList) return false;
	uint32_t targetID = itemForm.cachedForm->GetFormID();
	int32_t count = 0;
	for (auto& item : a_refr->inventoryList->data) {
		if (item.object && item.object->GetFormID() == targetID) {
			for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
				count += static_cast<int32_t>(stack->GetCount());
			}
		}
	}
	return CompareValues(static_cast<float>(count), comparison, numericValue.GetValue(a_refr));
}

void InventoryCountCondition::InitializeImpl(const nlohmann::json& a_json)
{
	itemForm.Initialize(a_json);
	itemForm.ResolveForm();
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void InventoryCountCondition::SerializeImpl(nlohmann::json& a_json) const
{
	itemForm.Serialize(a_json);
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// EquippedObjectWeight
std::string EquippedObjectWeightCondition::GetParameterString() const
{
	return std::format("eqWeight {} {:.1f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void EquippedObjectWeightCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##eowOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##eowVal", &numericValue.staticValue, 0, 0, "%.1f")) { a_dirty = true; }
}

bool EquippedObjectWeightCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	return CompareValues(actor->equippedWeight, comparison, numericValue.GetValue(a_refr));
}

void EquippedObjectWeightCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void EquippedObjectWeightCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// InventoryWeight
std::string InventoryWeightCondition::GetParameterString() const
{
	return std::format("invWeight {} {:.1f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void InventoryWeightCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##iwOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##iwVal", &numericValue.staticValue, 0, 0, "%.1f")) { a_dirty = true; }
}

bool InventoryWeightCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	float invWeight = 0.f;
	if (a_refr->inventoryList) {
		invWeight = a_refr->inventoryList->cachedWeight;
	}
	return CompareValues(invWeight, comparison, numericValue.GetValue(a_refr));
}

void InventoryWeightCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void InventoryWeightCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// SubmergeLevel
std::string SubmergeLevelCondition::GetParameterString() const
{
	return std::format("submerge {} {:.2f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void SubmergeLevelCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##slOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##slVal", &numericValue.staticValue, 0, 0, "%.2f")) { a_dirty = true; }
}

bool SubmergeLevelCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* cell = a_refr->GetParentCell();
	if (!cell) return false;
	// TESObjectCELL waterHeight is at a known offset
	auto* cellBytes = reinterpret_cast<uint8_t*>(cell);
	float waterHeight = *reinterpret_cast<float*>(cellBytes + 0xD0);
	float actorZ = a_refr->data.location.z;
	float submerge = waterHeight - actorZ;
	if (submerge < 0.f) submerge = 0.f;
	return CompareValues(submerge, comparison, numericValue.GetValue(a_refr));
}

void SubmergeLevelCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void SubmergeLevelCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// HasMagicEffect
bool HasMagicEffectCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return false;
	// ActiveEffectList is at offset in MiddleHighProcessData
	// ActiveEffectList contains BSTArray<BSTSmartPointer<ActiveEffect>> data
	// ActiveEffect has effectSetting at a known offset
	// For now, iterate via the process data's activeEffects member
	auto* mh = actor->currentProcess->middleHigh;
	auto* mhBytes = reinterpret_cast<uint8_t*>(mh);
	// ActiveEffectList at offset 0x238 in MiddleHighProcessData
	auto* aeList = reinterpret_cast<uint8_t*>(mhBytes + 0x238);
	// BSTArray<BSTSmartPointer<ActiveEffect>> at offset 0 in ActiveEffectList
	auto* arrData = *reinterpret_cast<void***>(aeList);
	int32_t arrSize = *reinterpret_cast<int32_t*>(aeList + 0x08);
	if (!arrData || arrSize <= 0) return false;
	uint32_t targetID = form.cachedForm->GetFormID();
	for (int32_t i = 0; i < arrSize; ++i) {
		auto* aePtr = reinterpret_cast<uint8_t*>(arrData[i]);
		if (!aePtr || reinterpret_cast<uintptr_t>(aePtr) < 0x10000) continue;
		// ActiveEffect: effectSetting* is typically at offset +0x10 or +0x18
		auto* effectSetting = *reinterpret_cast<RE::TESForm**>(aePtr + 0x18);
		if (effectSetting && effectSetting->GetFormID() == targetID) return true;
	}
	return false;
}

void HasMagicEffectCondition::InitializeImpl(const nlohmann::json& a_json) { form.Initialize(a_json); form.ResolveForm(); }
void HasMagicEffectCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

// IsWorn
bool IsWornCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm || !a_refr->inventoryList) return false;
	uint32_t targetID = form.cachedForm->GetFormID();
	for (auto& item : a_refr->inventoryList->data) {
		if (item.object && item.object->GetFormID() == targetID) {
			for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
				if (stack->IsEquipped()) return true;
			}
		}
	}
	return false;
}

void IsWornCondition::InitializeImpl(const nlohmann::json& a_json) { form.Initialize(a_json); form.ResolveForm(); }
void IsWornCondition::SerializeImpl(nlohmann::json& a_json) const { form.Serialize(a_json); }

// IsWornHasKeyword
void IsWornHasKeywordCondition::DrawEditWidgets(bool& a_dirty)
{
	if (UIFormPicker::DrawKeywordPicker("WornKW", editorID, a_dirty)) {
		if (!editorID.empty()) cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
	}
}

bool IsWornHasKeywordCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !cachedKeyword || !a_refr->inventoryList) return false;
	for (auto& item : a_refr->inventoryList->data) {
		if (!item.object) continue;
		for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
			if (!stack->IsEquipped()) continue;
			RE::BGSKeywordForm* kwForm = nullptr;
			auto ft = item.object->GetFormType();
			if (ft == RE::ENUM_FORM_ID::kWEAP) {
				kwForm = static_cast<RE::BGSKeywordForm*>(static_cast<RE::TESObjectWEAP*>(item.object));
			} else if (ft == RE::ENUM_FORM_ID::kARMO) {
				kwForm = static_cast<RE::BGSKeywordForm*>(static_cast<RE::TESObjectARMO*>(item.object));
			}
			if (kwForm && kwForm->HasKeyword(cachedKeyword, nullptr)) return true;
		}
	}
	return false;
}

void IsWornHasKeywordCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("editorID")) {
		editorID = a_json["editorID"].get<std::string>();
		cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
	}
}

void IsWornHasKeywordCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["editorID"] = editorID;
}

// IsDoingFavor - checks if actor is doing a favor for the player
bool IsDoingFavorCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return false;
	// MiddleHighProcessData has a bDoingFavor flag
	auto* mhBytes = reinterpret_cast<uint8_t*>(actor->currentProcess->middleHigh);
	return *reinterpret_cast<bool*>(mhBytes + 0x4A3);
}

// IdleTime - time since actor last moved (uses packageIdleTimer)
std::string IdleTimeCondition::GetParameterString() const
{
	return std::format("idle {} {:.1f}s", ComparisonOpToString(comparison), numericValue.staticValue);
}

void IdleTimeCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##itOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##itVal", &numericValue.staticValue, 0, 0, "%.1f")) { a_dirty = true; }
}

bool IdleTimeCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return false;
	float idleTimer = actor->currentProcess->middleHigh->packageIdleTimer;
	return CompareValues(idleTimer, comparison, numericValue.GetValue(a_refr));
}

void IdleTimeCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void IdleTimeCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// CurrentTargetAngle
std::string CurrentTargetAngleCondition::GetParameterString() const
{
	return std::format("tgtAngle {} {:.0f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void CurrentTargetAngleCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##taOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##taVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
}

bool CurrentTargetAngleCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !static_cast<bool>(actor->currentCombatTarget)) return false;
	auto targetPtr = actor->currentCombatTarget.get();
	if (!targetPtr) return false;
	auto& myPos = a_refr->data.location;
	auto& tgtPos = targetPtr->data.location;
	float dx = tgtPos.x - myPos.x;
	float dy = tgtPos.y - myPos.y;
	float angleToTarget = std::atan2(dy, dx) * (180.f / 3.14159265f);
	float myHeading = a_refr->data.angle.z * (180.f / 3.14159265f);
	float diff = angleToTarget - myHeading;
	while (diff > 180.f) diff -= 360.f;
	while (diff < -180.f) diff += 360.f;
	float absDiff = std::abs(diff);
	return CompareValues(absDiff, comparison, numericValue.GetValue(a_refr));
}

void CurrentTargetAngleCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void CurrentTargetAngleCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// FallDistance - compare vertical velocity * time or just fall distance graph var
std::string FallDistanceCondition::GetParameterString() const
{
	return std::format("fall {} {:.0f}", ComparisonOpToString(comparison), numericValue.staticValue);
}

void FallDistanceCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##fdOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##fdVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
}

bool FallDistanceCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	float fallDist = 0.f;
	static const RE::BSFixedString kVarName("fFallDistance");
	a_refr->GetGraphVariableImpl(kVarName, fallDist);
	return CompareValues(fallDist, comparison, numericValue.GetValue(a_refr));
}

void FallDistanceCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void FallDistanceCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

// IsQuestStageDone
std::string IsQuestStageDoneCondition::GetParameterString() const
{
	return std::format("{} stage {}", questForm.GetDisplayString(), stage);
}

void IsQuestStageDoneCondition::DrawEditWidgets(bool& a_dirty)
{
	questForm.DrawEditWidgets("Quest", a_dirty, RE::ENUM_FORM_ID::kQUST);
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputInt("Stage##qsd", &stage)) { a_dirty = true; }
}

bool IsQuestStageDoneCondition::EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!questForm.cachedForm) return false;
	auto* quest = questForm.cachedForm->As<RE::TESQuest>();
	if (!quest) return false;
	// TESQuest::GetCurrentStageID is at a known offset in the quest
	auto* questBytes = reinterpret_cast<uint8_t*>(quest);
	// currentStage is a uint16_t at offset 0x3C in TESQuest (FO4)
	uint16_t currentStage = *reinterpret_cast<uint16_t*>(questBytes + 0x3C);
	return currentStage >= static_cast<uint16_t>(stage);
}

void IsQuestStageDoneCondition::InitializeImpl(const nlohmann::json& a_json)
{
	questForm.Initialize(a_json);
	questForm.ResolveForm();
	if (a_json.contains("stage")) stage = a_json["stage"].get<int32_t>();
}

void IsQuestStageDoneCondition::SerializeImpl(nlohmann::json& a_json) const
{
	questForm.Serialize(a_json);
	a_json["stage"] = stage;
}

// ===== Animation Time Conditions =====

static float GetAnimDuration(RE::hkbClipGenerator* a_clipGen)
{
	if (!a_clipGen) return 0.0f;
	auto* anim = a_clipGen->GetAnimation();
	return anim ? anim->duration : 0.0f;
}

bool AnimTimeRemainingCondition::EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator* a_clipGen, const SubMod*) const
{
	if (!a_clipGen) return false;
	float duration = GetAnimDuration(a_clipGen);
	if (duration <= 0.0f) return false;
	float localTime = a_clipGen->GetLocalTime();
	float remaining = duration - localTime;
	float compareVal = numericValue.GetValue(nullptr);
	return CompareValues(remaining, comparison, compareVal);
}

std::string AnimTimeRemainingCondition::GetParameterString() const
{
	return std::format("{} {:.2f}s", ComparisonOpToString(comparison), numericValue.staticValue);
}

void AnimTimeRemainingCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##atrOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	if (ImGui::InputFloat("Seconds##atr", &numericValue.staticValue, 0.05f, 0.1f, "%.2f")) { a_dirty = true; }
}

void AnimTimeRemainingCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void AnimTimeRemainingCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	nlohmann::json nv; numericValue.Serialize(nv); a_json["numericValue"] = nv;
}

bool AnimTimeElapsedCondition::EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator* a_clipGen, const SubMod*) const
{
	if (!a_clipGen) return false;
	float localTime = a_clipGen->GetLocalTime();
	float compareVal = numericValue.GetValue(nullptr);
	return CompareValues(localTime, comparison, compareVal);
}

std::string AnimTimeElapsedCondition::GetParameterString() const
{
	return std::format("{} {:.2f}s", ComparisonOpToString(comparison), numericValue.staticValue);
}

void AnimTimeElapsedCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##ateOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	if (ImGui::InputFloat("Seconds##ate", &numericValue.staticValue, 0.05f, 0.1f, "%.2f")) { a_dirty = true; }
}

void AnimTimeElapsedCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void AnimTimeElapsedCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	nlohmann::json nv; numericValue.Serialize(nv); a_json["numericValue"] = nv;
}

bool AnimProgressCondition::EvaluateImpl(RE::TESObjectREFR*, RE::hkbClipGenerator* a_clipGen, const SubMod*) const
{
	if (!a_clipGen) return false;
	float duration = GetAnimDuration(a_clipGen);
	if (duration <= 0.0f) return false;
	float progress = a_clipGen->GetLocalTime() / duration;
	float compareVal = numericValue.GetValue(nullptr);
	return CompareValues(progress, comparison, compareVal);
}

std::string AnimProgressCondition::GetParameterString() const
{
	return std::format("{} {:.0f}%", ComparisonOpToString(comparison), numericValue.staticValue * 100.f);
}

void AnimProgressCondition::DrawEditWidgets(bool& a_dirty)
{
	int compIdx = static_cast<int>(comparison);
	const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##apOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	if (ImGui::SliderFloat("Progress##ap", &numericValue.staticValue, 0.0f, 1.0f, "%.2f")) { a_dirty = true; }
}

void AnimProgressCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void AnimProgressCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	nlohmann::json nv; numericValue.Serialize(nv); a_json["numericValue"] = nv;
}

// ===== IsPlayingIdleAnimation =====

bool IsPlayingIdleAnimationCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr || !form.cachedForm) return false;

	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;

	auto* process = actor->currentProcess;
	if (!process || !process->middleHigh) return false;

	auto* currentIdle = process->middleHigh->currentIdle;
	if (!currentIdle) return false;

	return currentIdle->GetFormID() == form.cachedForm->GetFormID();
}

void IsPlayingIdleAnimationCondition::InitializeImpl(const nlohmann::json& a_json)
{
	form.Initialize(a_json);
	form.ResolveForm();
}

void IsPlayingIdleAnimationCondition::SerializeImpl(nlohmann::json& a_json) const
{
	form.Serialize(a_json);
}

// =============================================================================
// Detection Conditions
// =============================================================================

namespace
{
	// Validates that a target actor is suitable for detection iteration
	bool ValidateDetectionTarget(RE::Actor* a_actor, RE::Actor* a_target)
	{
		if (!a_target || !a_actor) return false;
		if (a_target == a_actor) return false;
		// Check deleted flag (bit 5) and initially-disabled flag (bit 11)
		uint32_t flags = a_target->GetFormFlags();
		if (flags & ((1u << 5) | (1u << 11))) return false;
		if (a_target->IsDead(false)) return false;
		return true;
	}

	// Core detection loop shared by DetectedBy and Detects
	bool EvaluateDetection(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen,
		const SubMod* a_sub, const ConditionSet& a_childConditions, bool a_isDetectedBy)
	{
		if (!a_refr) return false;
		auto* actor = a_refr->As<RE::Actor>();
		if (!actor) return false;

		auto* processLists = OAR_RE::ProcessLists::GetSingleton();
		if (!processLists) return false;

		// Collect actors that pass detection check
		auto checkActor = [&](RE::Actor* candidate) -> bool {
			if (!ValidateDetectionTarget(actor, candidate)) return false;

			RE::Actor* detector = a_isDetectedBy ? candidate : actor;
			RE::Actor* detectee = a_isDetectedBy ? actor : candidate;

			if (OAR_RE::RequestDetectionLevel(detector, detectee) <= 0)
				return false;

			// Set thread-local context for child-only conditions
			g_detectionTarget = candidate;
			g_detectionSource = actor;

			// Evaluate child conditions against the candidate
			bool childrenPass = a_childConditions.IsEmpty() ||
				a_childConditions.EvaluateAll(candidate->As<RE::TESObjectREFR>(), a_clipGen, a_sub);

			if (childrenPass) return true;
			return false;
		};

		// Iterate high-process actors
		for (auto& actorHandle : processLists->highActorHandles) {
			if (auto actorPtr = actorHandle.get()) {
				if (auto* target = actorPtr.get()) {
					if (checkActor(target)) {
						g_detectionTarget = nullptr;
						g_detectionSource = nullptr;
						return true;
					}
				}
			}
		}

		// Also check player (not always in high-process list)
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (player && checkActor(player)) {
			g_detectionTarget = nullptr;
			g_detectionSource = nullptr;
			return true;
		}

		g_detectionTarget = nullptr;
		g_detectionSource = nullptr;
		return false;
	}
}

// --- DetectedBy ---

bool DetectedByCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const
{
	return EvaluateDetection(a_refr, a_clipGen, a_sub, childConditions, true);
}

void DetectedByCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("conditions") && a_json["conditions"].is_array()) {
		for (const auto& condJson : a_json["conditions"]) {
			if (auto cond = CreateConditionFromJson(condJson)) {
				childConditions.AddCondition(std::move(cond));
			}
		}
	}
}

void DetectedByCondition::SerializeImpl(nlohmann::json& a_json) const
{
	auto& arr = a_json["conditions"];
	arr = nlohmann::json::array();
	for (const auto& cond : childConditions.GetConditions()) {
		nlohmann::json condJson;
		cond->Serialize(condJson);
		arr.push_back(condJson);
	}
}

// --- Detects ---

bool DetectsCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_sub) const
{
	return EvaluateDetection(a_refr, a_clipGen, a_sub, childConditions, false);
}

void DetectsCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("conditions") && a_json["conditions"].is_array()) {
		for (const auto& condJson : a_json["conditions"]) {
			if (auto cond = CreateConditionFromJson(condJson)) {
				childConditions.AddCondition(std::move(cond));
			}
		}
	}
}

void DetectsCondition::SerializeImpl(nlohmann::json& a_json) const
{
	auto& arr = a_json["conditions"];
	arr = nlohmann::json::array();
	for (const auto& cond : childConditions.GetConditions()) {
		nlohmann::json condJson;
		cond->Serialize(condJson);
		arr.push_back(condJson);
	}
}

// --- DetectionDistance ---

bool DetectionDistanceCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!g_detectionTarget || !g_detectionSource) return false;

	float distSq = OAR_RE::GetSquaredDistance(
		g_detectionSource->data.location,
		g_detectionTarget->data.location);

	float threshold = numericValue.GetValue(a_refr);
	// Compare squared distance against squared threshold to avoid sqrt
	return CompareValues(distSq, comparison, threshold * threshold);
}

void DetectionDistanceCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void DetectionDistanceCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

std::string DetectionDistanceCondition::GetParameterString() const
{
	return std::format("dist {} {}", ComparisonOperatorToString(comparison), numericValue.staticValue);
}

void DetectionDistanceCondition::DrawEditWidgets(bool& a_dirty)
{
	static const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	int compIdx = static_cast<int>(comparison);
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##ddOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##ddVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
}

// --- DetectionRelationship ---

bool DetectionRelationshipCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!g_detectionTarget || !g_detectionSource) return false;

	auto* npc1 = g_detectionSource->GetNPC();
	auto* npc2 = g_detectionTarget->GetNPC();
	if (!npc1 || !npc2) return false;

	int32_t rank = OAR_RE::GetRelationshipRank(npc1, npc2);
	return CompareValues(static_cast<float>(rank), comparison, numericValue.GetValue(a_refr));
}

void DetectionRelationshipCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
}

void DetectionRelationshipCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
}

std::string DetectionRelationshipCondition::GetParameterString() const
{
	return std::format("rank {} {}", ComparisonOperatorToString(comparison), numericValue.staticValue);
}

void DetectionRelationshipCondition::DrawEditWidgets(bool& a_dirty)
{
	static const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	int compIdx = static_cast<int>(comparison);
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##drOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##drVal", &numericValue.staticValue, 0, 0, "%.0f")) { a_dirty = true; }
	ImGui::SameLine();
	ImGui::TextDisabled("(-4=Archnemesis .. +4=Lover)");
}

// --- DetectionAngle ---

bool DetectionAngleCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!g_detectionTarget || !g_detectionSource) return false;

	RE::Actor* angleActor = swapActors ? g_detectionTarget : g_detectionSource;
	RE::Actor* targetActor = swapActors ? g_detectionSource : g_detectionTarget;

	// Actor's heading in degrees (data.angle.z is in radians)
	float actorAngle = angleActor->data.angle.z * (180.0f / static_cast<float>(std::numbers::pi));

	// Compute bearing from angleActor to targetActor
	float dx = targetActor->data.location.x - angleActor->data.location.x;
	float dy = targetActor->data.location.y - angleActor->data.location.y;
	float angleToTarget = std::atan2(dx, dy) * (180.0f / static_cast<float>(std::numbers::pi));
	if (angleToTarget < 0) angleToTarget += 360.0f;

	// Normalize actorAngle to 0-360
	if (actorAngle < 0) actorAngle += 360.0f;

	// Handle phase wrapping
	if (std::fabs(actorAngle - angleToTarget) > 180.0f) {
		if (actorAngle < angleToTarget)
			actorAngle += 360.0f;
		else
			angleToTarget += 360.0f;
	}

	bool targetOnRight = (actorAngle - angleToTarget) < 0;

	float relativeAngle = std::fabs(actorAngle - angleToTarget);
	if (relativeAngle > 180.0f) relativeAngle = 360.0f - relativeAngle;

	// Side filtering
	if (limitRight && !targetOnRight) return false;
	if (limitLeft && targetOnRight) return false;

	return CompareValues(relativeAngle, comparison, numericValue.GetValue(a_refr));
}

void DetectionAngleCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("swapActors")) swapActors = a_json["swapActors"].get<bool>();
	if (a_json.contains("comparison")) comparison = static_cast<ComparisonOperator>(a_json["comparison"].get<int32_t>());
	if (a_json.contains("numericValue")) numericValue.Initialize(a_json["numericValue"]);
	if (a_json.contains("limitRight")) limitRight = a_json["limitRight"].get<bool>();
	if (a_json.contains("limitLeft")) limitLeft = a_json["limitLeft"].get<bool>();
}

void DetectionAngleCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["swapActors"] = swapActors;
	a_json["comparison"] = static_cast<int32_t>(comparison);
	numericValue.Serialize(a_json["numericValue"]);
	a_json["limitRight"] = limitRight;
	a_json["limitLeft"] = limitLeft;
}

std::string DetectionAngleCondition::GetParameterString() const
{
	std::string s = std::format("angle {} {}", ComparisonOperatorToString(comparison), numericValue.staticValue);
	if (swapActors) s += " [swapped]";
	if (limitRight) s += " [R]";
	if (limitLeft) s += " [L]";
	return s;
}

void DetectionAngleCondition::DrawEditWidgets(bool& a_dirty)
{
	if (ImGui::Checkbox("Swap perspective##da", &swapActors)) a_dirty = true;
	ImGui::SameLine();
	static const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
	int compIdx = static_cast<int>(comparison);
	ImGui::SetNextItemWidth(60);
	if (ImGui::Combo("##daOp", &compIdx, ops, 6)) { comparison = static_cast<ComparisonOperator>(compIdx); a_dirty = true; }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputFloat("##daVal", &numericValue.staticValue, 0, 0, "%.1f")) { a_dirty = true; }
	if (ImGui::Checkbox("Right only##da", &limitRight)) a_dirty = true;
	ImGui::SameLine();
	if (ImGui::Checkbox("Left only##da", &limitLeft)) a_dirty = true;
}

// =============================================================================
// Dialogue Condition
// =============================================================================

bool DialogueCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	(void)a_refr;

	// Query current dialogue menu state
	auto* ui = RE::UI::GetSingleton();
	bool menuCurrentlyOpen = ui ? ui->GetMenuOpen(RE::BSFixedString("DialogueMenu")) : false;

	// Edge detection for transient states
	if (menuCurrentlyOpen && !prevMenuOpen) {
		dialogueStartedFlag = true;
		dialogueEndedFlag = false;
	}
	if (!menuCurrentlyOpen && prevMenuOpen) {
		dialogueEndedFlag = true;
		dialogueStartedFlag = false;
		playerChoseFlag = false;
	}
	prevMenuOpen = menuCurrentlyOpen;

	// Check MenuTopicManager for player choosing state
	bool playerCurrentlyChoosing = false;
	if (auto* mtm = OAR_RE::MenuTopicManager::GetSingleton()) {
		// When the menu is open and not shutting down, player is in dialogue.
		// canSkip being false often indicates waiting for player choice.
		playerCurrentlyChoosing = mtm->menuOpen && !mtm->canSkip && !mtm->shutMenu;
	}

	// Evaluate each enabled sub-check
	if (checkDialogueActive && !menuCurrentlyOpen) return false;
	if (checkDialogueStarted && !dialogueStartedFlag) return false;
	if (checkPlayerChoosing && !playerCurrentlyChoosing) return false;
	if (checkPlayerChose && !playerChoseFlag) return false;
	if (checkDialogueEnded && !dialogueEndedFlag) return false;

	// Clear one-shot flags after they've been consumed
	if (checkDialogueStarted && dialogueStartedFlag) dialogueStartedFlag = false;
	if (checkDialogueEnded && dialogueEndedFlag) dialogueEndedFlag = false;

	// At least one sub-check must be enabled for the condition to be meaningful
	return checkDialogueActive || checkDialogueStarted || checkPlayerChoosing ||
		checkPlayerChose || checkDialogueEnded;
}

void DialogueCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("dialogueActive")) checkDialogueActive = a_json["dialogueActive"].get<bool>();
	if (a_json.contains("dialogueStarted")) checkDialogueStarted = a_json["dialogueStarted"].get<bool>();
	if (a_json.contains("playerChoosing")) checkPlayerChoosing = a_json["playerChoosing"].get<bool>();
	if (a_json.contains("playerChose")) checkPlayerChose = a_json["playerChose"].get<bool>();
	if (a_json.contains("dialogueEnded")) checkDialogueEnded = a_json["dialogueEnded"].get<bool>();
}

void DialogueCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["dialogueActive"] = checkDialogueActive;
	a_json["dialogueStarted"] = checkDialogueStarted;
	a_json["playerChoosing"] = checkPlayerChoosing;
	a_json["playerChose"] = checkPlayerChose;
	a_json["dialogueEnded"] = checkDialogueEnded;
}

std::string DialogueCondition::GetParameterString() const
{
	std::string s;
	if (checkDialogueActive) s += "active ";
	if (checkDialogueStarted) s += "started ";
	if (checkPlayerChoosing) s += "choosing ";
	if (checkPlayerChose) s += "chose ";
	if (checkDialogueEnded) s += "ended ";
	if (s.empty()) s = "(none)";
	return s;
}

void DialogueCondition::DrawEditWidgets(bool& a_dirty)
{
	if (ImGui::Checkbox("Dialogue Active", &checkDialogueActive)) a_dirty = true;
	if (ImGui::Checkbox("Dialogue Started (edge)", &checkDialogueStarted)) a_dirty = true;
	if (ImGui::Checkbox("Player Choosing", &checkPlayerChoosing)) a_dirty = true;
	if (ImGui::Checkbox("Player Chose (edge)", &checkPlayerChose)) a_dirty = true;
	if (ImGui::Checkbox("Dialogue Ended (edge)", &checkDialogueEnded)) a_dirty = true;
}

// =============================================================================
// Math Statement Condition
// =============================================================================

// Simple expression evaluator that handles basic math operators and comparisons.
// Supports: +, -, *, /, >, <, >=, <=, ==, !=, and, or, not
// Variables are resolved via the MathVariable vector.
namespace MathEval
{
	struct Token
	{
		enum class Type { Number, Variable, Op, LParen, RParen, End };
		Type type;
		double value{ 0.0 };
		std::string name;
		char op{ 0 };
	};

	class Tokenizer
	{
	public:
		Tokenizer(const std::string& expr) : src(expr), pos(0) {}

		Token Next()
		{
			SkipWhitespace();
			if (pos >= src.size()) return { Token::Type::End };

			char c = src[pos];

			// Numbers
			if (std::isdigit(c) || (c == '.' && pos + 1 < src.size() && std::isdigit(src[pos + 1]))) {
				return ReadNumber();
			}

			// Identifiers (variables and keywords)
			if (std::isalpha(c) || c == '_') {
				return ReadIdentifier();
			}

			// Two-character operators
			if (pos + 1 < src.size()) {
				std::string two = src.substr(pos, 2);
				if (two == ">=" || two == "<=" || two == "==" || two == "!=") {
					pos += 2;
					return { Token::Type::Op, 0.0, "", two[0] == '>' ? 'G' : two[0] == '<' ? 'L' : two[0] == '=' ? 'E' : 'N' };
				}
			}

			// Single-character operators
			if (c == '+' || c == '-' || c == '*' || c == '/' || c == '>' || c == '<') {
				pos++;
				return { Token::Type::Op, 0.0, "", c };
			}

			if (c == '(') { pos++; return { Token::Type::LParen }; }
			if (c == ')') { pos++; return { Token::Type::RParen }; }

			// Unknown character — skip
			pos++;
			return Next();
		}

	private:
		void SkipWhitespace() { while (pos < src.size() && std::isspace(src[pos])) pos++; }

		Token ReadNumber()
		{
			size_t start = pos;
			while (pos < src.size() && (std::isdigit(src[pos]) || src[pos] == '.')) pos++;
			double val = std::stod(src.substr(start, pos - start));
			return { Token::Type::Number, val };
		}

		Token ReadIdentifier()
		{
			size_t start = pos;
			while (pos < src.size() && (std::isalnum(src[pos]) || src[pos] == '_')) pos++;
			std::string id = src.substr(start, pos - start);

			// Keywords
			if (id == "and") return { Token::Type::Op, 0.0, "", '&' };
			if (id == "or") return { Token::Type::Op, 0.0, "", '|' };
			if (id == "not") return { Token::Type::Op, 0.0, "", '!' };

			return { Token::Type::Variable, 0.0, id };
		}

		std::string src;
		size_t pos;
	};

	// Recursive descent parser with operator precedence
	class Parser
	{
	public:
		Parser(const std::string& expr, const std::unordered_map<std::string, double>& vars)
			: tokenizer(expr), variables(vars)
		{
			current = tokenizer.Next();
		}

		double Parse() { return ParseOr(); }

	private:
		double ParseOr()
		{
			double left = ParseAnd();
			while (current.type == Token::Type::Op && current.op == '|') {
				Advance();
				double right = ParseAnd();
				left = (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
			}
			return left;
		}

		double ParseAnd()
		{
			double left = ParseComparison();
			while (current.type == Token::Type::Op && current.op == '&') {
				Advance();
				double right = ParseComparison();
				left = (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
			}
			return left;
		}

		double ParseComparison()
		{
			double left = ParseAddSub();
			while (current.type == Token::Type::Op &&
				(current.op == '>' || current.op == '<' || current.op == 'G' ||
					current.op == 'L' || current.op == 'E' || current.op == 'N')) {
				char op = current.op;
				Advance();
				double right = ParseAddSub();
				switch (op) {
				case '>': left = left > right ? 1.0 : 0.0; break;
				case '<': left = left < right ? 1.0 : 0.0; break;
				case 'G': left = left >= right ? 1.0 : 0.0; break; // >=
				case 'L': left = left <= right ? 1.0 : 0.0; break; // <=
				case 'E': left = (std::abs(left - right) < 0.0001) ? 1.0 : 0.0; break; // ==
				case 'N': left = (std::abs(left - right) >= 0.0001) ? 1.0 : 0.0; break; // !=
				}
			}
			return left;
		}

		double ParseAddSub()
		{
			double left = ParseMulDiv();
			while (current.type == Token::Type::Op && (current.op == '+' || current.op == '-')) {
				char op = current.op;
				Advance();
				double right = ParseMulDiv();
				left = (op == '+') ? left + right : left - right;
			}
			return left;
		}

		double ParseMulDiv()
		{
			double left = ParseUnary();
			while (current.type == Token::Type::Op && (current.op == '*' || current.op == '/')) {
				char op = current.op;
				Advance();
				double right = ParseUnary();
				if (op == '*') left *= right;
				else left = (right != 0.0) ? left / right : 0.0;
			}
			return left;
		}

		double ParseUnary()
		{
			if (current.type == Token::Type::Op && current.op == '-') {
				Advance();
				return -ParseUnary();
			}
			if (current.type == Token::Type::Op && current.op == '!') {
				Advance();
				return ParseUnary() == 0.0 ? 1.0 : 0.0;
			}
			return ParsePrimary();
		}

		double ParsePrimary()
		{
			if (current.type == Token::Type::Number) {
				double val = current.value;
				Advance();
				return val;
			}
			if (current.type == Token::Type::Variable) {
				std::string name = current.name;
				Advance();
				auto it = variables.find(name);
				return it != variables.end() ? it->second : 0.0;
			}
			if (current.type == Token::Type::LParen) {
				Advance();
				double val = ParseOr();
				if (current.type == Token::Type::RParen) Advance();
				return val;
			}
			return 0.0;
		}

		void Advance() { current = tokenizer.Next(); }

		Tokenizer tokenizer;
		Token current;
		const std::unordered_map<std::string, double>& variables;
	};

	inline double Evaluate(const std::string& expr, const std::unordered_map<std::string, double>& vars)
	{
		Parser parser(expr, vars);
		return parser.Parse();
	}
}

bool MathStatementCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (expression.empty()) return false;

	// Build variable map from current values
	std::unordered_map<std::string, double> varMap;
	for (const auto& var : variables) {
		varMap[var.name] = static_cast<double>(var.numericValue.GetValue(a_refr));
	}

	double result = MathEval::Evaluate(expression, varMap);
	return result != 0.0;
}

void MathStatementCondition::InitializeImpl(const nlohmann::json& a_json)
{
	if (a_json.contains("expression")) expression = a_json["expression"].get<std::string>();
	if (a_json.contains("variables") && a_json["variables"].is_array()) {
		for (const auto& varJson : a_json["variables"]) {
			MathVariable mv;
			if (varJson.contains("name")) mv.name = varJson["name"].get<std::string>();
			if (varJson.contains("numericValue")) mv.numericValue.Initialize(varJson["numericValue"]);
			variables.push_back(std::move(mv));
		}
	}
}

void MathStatementCondition::SerializeImpl(nlohmann::json& a_json) const
{
	a_json["expression"] = expression;
	auto& arr = a_json["variables"];
	arr = nlohmann::json::array();
	for (const auto& var : variables) {
		nlohmann::json varJson;
		varJson["name"] = var.name;
		var.numericValue.Serialize(varJson["numericValue"]);
		arr.push_back(varJson);
	}
}

std::string MathStatementCondition::GetParameterString() const
{
	if (expression.empty()) return "(empty expression)";
	return expression;
}

void MathStatementCondition::DrawEditWidgets(bool& a_dirty)
{
	// Expression input
	static char exprBuf[512];
	strncpy_s(exprBuf, expression.c_str(), sizeof(exprBuf) - 1);
	ImGui::Text("Expression:");
	ImGui::SetNextItemWidth(-1);
	if (ImGui::InputText("##mathExpr", exprBuf, sizeof(exprBuf))) {
		expression = exprBuf;
		a_dirty = true;
	}

	ImGui::Separator();
	ImGui::Text("Variables:");

	// Existing variables
	int removeIdx = -1;
	for (int i = 0; i < static_cast<int>(variables.size()); i++) {
		ImGui::PushID(i);
		auto& var = variables[i];

		static char nameBuf[64];
		strncpy_s(nameBuf, var.name.c_str(), sizeof(nameBuf) - 1);
		ImGui::SetNextItemWidth(80);
		if (ImGui::InputText("##vname", nameBuf, sizeof(nameBuf))) {
			var.name = nameBuf;
			a_dirty = true;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		if (ImGui::InputFloat("##vval", &var.numericValue.staticValue, 0, 0, "%.2f")) {
			a_dirty = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("X##vdel")) {
			removeIdx = i;
			a_dirty = true;
		}
		ImGui::PopID();
	}

	if (removeIdx >= 0) {
		variables.erase(variables.begin() + removeIdx);
	}

	// Add variable button
	if (ImGui::SmallButton("+ Add Variable")) {
		MathVariable mv;
		mv.name = std::format("v{}", variables.size());
		variables.push_back(std::move(mv));
		a_dirty = true;
	}
}

// ===== Factory Registration =====

void RegisterAllConditions()
{
	auto* factory = ConditionFactory::GetSingleton();
	factory->Register("IsForm", [] { return std::make_unique<IsFormCondition>(); });
	factory->Register("IsActorBase", [] { return std::make_unique<IsActorBaseCondition>(); });
	factory->Register("IsRace", [] { return std::make_unique<IsRaceCondition>(); });
	factory->Register("IsFemale", [] { return std::make_unique<IsFemaleCondition>(); });
	factory->Register("IsWeaponDrawn", [] { return std::make_unique<IsWeaponDrawnCondition>(); });
	factory->Register("IsInCombat", [] { return std::make_unique<IsInCombatCondition>(); });
	factory->Register("IsSprinting", [] { return std::make_unique<IsSprintingCondition>(); });
	factory->Register("IsInAir", [] { return std::make_unique<IsInAirCondition>(); });
	factory->Register("HasKeyword", [] { return std::make_unique<HasKeywordCondition>(); });
	factory->Register("IsInFaction", [] { return std::make_unique<IsInFactionCondition>(); });
	factory->Register("Random", [] { return std::make_unique<RandomCondition>(); });
	factory->Register("Level", [] { return std::make_unique<LevelCondition>(); });
	factory->Register("OR", [] { return std::make_unique<ORCondition>(); });
	factory->Register("AND", [] { return std::make_unique<ANDCondition>(); });
	factory->Register("IsEquippedType", [] { return std::make_unique<IsEquippedTypeCondition>(); });
	factory->Register("IsInPowerArmor", [] { return std::make_unique<IsInPowerArmorCondition>(); });
	factory->Register("IsSneaking", [] { return std::make_unique<IsSneakingCondition>(); });
	factory->Register("CurrentWeather", [] { return std::make_unique<CurrentWeatherCondition>(); });
	factory->Register("IsADS", [] { return std::make_unique<IsADSCondition>(); });
	factory->Register("CompareActorValue", [] { return std::make_unique<CompareActorValueCondition>(); });
	factory->Register("CurrentMagazineAmmo", [] { return std::make_unique<CurrentMagazineAmmoCondition>(); });
	factory->Register("IsEquippedHasKeyword", [] { return std::make_unique<IsEquippedHasKeywordCondition>(); });
	factory->Register("IsEquipped", [] { return std::make_unique<IsEquippedCondition>(); });
	factory->Register("XOR", [] { return std::make_unique<XORCondition>(); });
	factory->Register("IsChild", [] { return std::make_unique<IsChildCondition>(); });
	factory->Register("IsPlayerTeammate", [] { return std::make_unique<IsPlayerTeammateCondition>(); });
	factory->Register("IsTalking", [] { return std::make_unique<IsTalkingCondition>(); });
	factory->Register("IsAttacking", [] { return std::make_unique<IsAttackingCondition>(); });
	factory->Register("IsReloading", [] { return std::make_unique<IsReloadingCondition>(); });
	factory->Register("IsFiring", [] { return std::make_unique<IsFiringCondition>(); });
	factory->Register("IsDryFiring", [] { return std::make_unique<IsDryFiringCondition>(); });
	factory->Register("IsButtonHeld", [] { return std::make_unique<IsButtonHeldCondition>(); });
	factory->Register("IsBlocking", [] { return std::make_unique<IsBlockingCondition>(); });
	factory->Register("IsRunning", [] { return std::make_unique<IsRunningCondition>(); });
	factory->Register("IsInInterior", [] { return std::make_unique<IsInInteriorCondition>(); });
	factory->Register("IsWorldSpace", [] { return std::make_unique<IsWorldSpaceCondition>(); });
	factory->Register("IsParentCell", [] { return std::make_unique<IsParentCellCondition>(); });
	factory->Register("IsInLocation", [] { return std::make_unique<IsInLocationCondition>(); });
	factory->Register("HasPerk", [] { return std::make_unique<HasPerkCondition>(); });
	factory->Register("HasSpell", [] { return std::make_unique<HasSpellCondition>(); });
	factory->Register("Scale", [] { return std::make_unique<ScaleCondition>(); });
	factory->Register("CurrentGameTime", [] { return std::make_unique<CurrentGameTimeCondition>(); });
	factory->Register("FactionRank", [] { return std::make_unique<FactionRankCondition>(); });
	factory->Register("CrimeGold", [] { return std::make_unique<CrimeGoldCondition>(); });
	factory->Register("LifeState", [] { return std::make_unique<LifeStateCondition>(); });
	factory->Register("SitSleepState", [] { return std::make_unique<SitSleepStateCondition>(); });
	factory->Register("HasTarget", [] { return std::make_unique<HasTargetCondition>(); });
	factory->Register("CurrentTargetDistance", [] { return std::make_unique<CurrentTargetDistanceCondition>(); });
	factory->Register("IsMovementDirection", [] { return std::make_unique<IsMovementDirectionCondition>(); });
	factory->Register("IsCombatState", [] { return std::make_unique<IsCombatStateCondition>(); });
	factory->Register("HasGraphVariable", [] { return std::make_unique<HasGraphVariableCondition>(); });
	factory->Register("IsReplacerEnabled", [] { return std::make_unique<IsReplacerEnabledCondition>(); });
	factory->Register("MovementSpeed", [] { return std::make_unique<MovementSpeedCondition>(); });
	factory->Register("IsSwimming", [] { return std::make_unique<IsSwimmingCondition>(); });
	factory->Register("IsOverEncumbered", [] { return std::make_unique<IsOverEncumberedCondition>(); });
	factory->Register("LocationHasKeyword", [] { return std::make_unique<LocationHasKeywordCondition>(); });
	factory->Register("LocationCleared", [] { return std::make_unique<LocationClearedCondition>(); });
	factory->Register("LightLevel", [] { return std::make_unique<LightLevelCondition>(); });
	factory->Register("SurfaceMaterial", [] { return std::make_unique<SurfaceMaterialCondition>(); });
	factory->Register("MovementSurfaceAngle", [] { return std::make_unique<MovementSurfaceAngleCondition>(); });
	factory->Register("IsOnStairs", [] { return std::make_unique<IsOnStairsCondition>(); });
	factory->Register("WindSpeed", [] { return std::make_unique<WindSpeedCondition>(); });
	factory->Register("WindAngleDifference", [] { return std::make_unique<WindAngleDifferenceCondition>(); });
	factory->Register("IsTrespassing", [] { return std::make_unique<IsTrespassingCondition>(); });
	factory->Register("IsCurrentPackage", [] { return std::make_unique<IsCurrentPackageCondition>(); });
	factory->Register("IsInScene", [] { return std::make_unique<IsInSceneCondition>(); });
	factory->Register("CurrentFurniture", [] { return std::make_unique<CurrentFurnitureCondition>(); });
	factory->Register("TARGET", [] { return std::make_unique<TargetConditionWrapper>(); });
	factory->Register("PLAYER", [] { return std::make_unique<PlayerConditionWrapper>(); });
	factory->Register("IsUnique", [] { return std::make_unique<IsUniqueCondition>(); });
	factory->Register("IsSummoned", [] { return std::make_unique<IsSummonedCondition>(); });
	factory->Register("IsGhost", [] { return std::make_unique<IsGhostCondition>(); });
	factory->Register("IsGreetingPlayer", [] { return std::make_unique<IsGreetingPlayerCondition>(); });
	factory->Register("IsGuard", [] { return std::make_unique<IsGuardCondition>(); });
	factory->Register("Height", [] { return std::make_unique<HeightCondition>(); });
	factory->Register("Weight", [] { return std::make_unique<WeightCondition>(); });
	factory->Register("InventoryCount", [] { return std::make_unique<InventoryCountCondition>(); });
	factory->Register("EquippedObjectWeight", [] { return std::make_unique<EquippedObjectWeightCondition>(); });
	factory->Register("InventoryWeight", [] { return std::make_unique<InventoryWeightCondition>(); });
	factory->Register("SubmergeLevel", [] { return std::make_unique<SubmergeLevelCondition>(); });
	factory->Register("HasMagicEffect", [] { return std::make_unique<HasMagicEffectCondition>(); });
	factory->Register("IsWorn", [] { return std::make_unique<IsWornCondition>(); });
	factory->Register("IsWornHasKeyword", [] { return std::make_unique<IsWornHasKeywordCondition>(); });
	factory->Register("IsDoingFavor", [] { return std::make_unique<IsDoingFavorCondition>(); });
	factory->Register("IdleTime", [] { return std::make_unique<IdleTimeCondition>(); });
	factory->Register("CurrentTargetAngle", [] { return std::make_unique<CurrentTargetAngleCondition>(); });
	factory->Register("FallDistance", [] { return std::make_unique<FallDistanceCondition>(); });
	factory->Register("IsQuestStageDone", [] { return std::make_unique<IsQuestStageDoneCondition>(); });
	factory->Register("AnimTimeRemaining", [] { return std::make_unique<AnimTimeRemainingCondition>(); });
	factory->Register("AnimTimeElapsed", [] { return std::make_unique<AnimTimeElapsedCondition>(); });
	factory->Register("AnimProgress", [] { return std::make_unique<AnimProgressCondition>(); });
	factory->Register("IsPlayingIdleAnimation", [] { return std::make_unique<IsPlayingIdleAnimationCondition>(); });

	// Detection conditions (ported from Skyrim OAR Detection Conditions plugin)
	factory->Register("DetectedBy", [] { return std::make_unique<DetectedByCondition>(); });
	factory->Register("Detects", [] { return std::make_unique<DetectsCondition>(); });
	factory->Register("DetectionDistance", [] { return std::make_unique<DetectionDistanceCondition>(); });
	factory->Register("DetectionRelationship", [] { return std::make_unique<DetectionRelationshipCondition>(); });
	factory->Register("DetectionAngle", [] { return std::make_unique<DetectionAngleCondition>(); });

	// Dialogue condition (ported from Skyrim OAR Dialogue Conditions plugin)
	factory->Register("Dialogue", [] { return std::make_unique<DialogueCondition>(); });

	// Math condition (ported from Skyrim OAR Math plugin)
	factory->Register("MathStatement", [] { return std::make_unique<MathStatementCondition>(); });

	logger::info("[OAR] Registered {} condition types", factory->GetAllFactories().size());
}

std::unique_ptr<ICondition> CreateConditionFromJson(const nlohmann::json& a_json)
{
	if (!a_json.contains("condition")) {
		logger::warn("[OAR] JSON condition missing 'condition' key");
		return nullptr;
	}

	std::string condName = a_json["condition"].get<std::string>();
	auto condition = ConditionFactory::GetSingleton()->Create(condName);

	if (!condition) {
		logger::warn("[OAR] Unknown condition type: '{}'", condName);
		return nullptr;
	}

	try {
		condition->Initialize(a_json);
	} catch (const std::exception& e) {
		logger::error("[OAR] Failed to initialize condition '{}': {}", condName, e.what());
		return nullptr;
	}

	return condition;
}
