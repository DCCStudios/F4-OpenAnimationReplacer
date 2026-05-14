#include "Conditions.h"
#include "Utils.h"
#include <imgui.h>
#include "RE/Bethesda/ActorValueInfo.h"

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

void FormComponent::DrawEditWidgets(const char* a_label, bool& a_dirty)
{
	char plugBuf[256]{};
	strncpy_s(plugBuf, pluginName.c_str(), sizeof(plugBuf) - 1);
	ImGui::SetNextItemWidth(200);
	if (ImGui::InputText(std::format("Plugin##{}", a_label).c_str(), plugBuf, sizeof(plugBuf))) {
		pluginName = plugBuf;
		a_dirty = true;
	}

	char formBuf[32]{};
	snprintf(formBuf, sizeof(formBuf), "0x%X", localFormID);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	if (ImGui::InputText(std::format("FormID##{}", a_label).c_str(), formBuf, sizeof(formBuf))) {
		try { localFormID = std::stoul(formBuf, nullptr, 16); } catch (...) {}
		a_dirty = true;
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
	// TESNPC inherits TESActorBaseData which has ACTOR_BASE_DATA at a known offset.
	// ACTOR_BASE_DATA::actorBaseFlags bit 0 = female. Access via raw offset from base form.
	// TESNPC layout: TESForm(0x20) + ... + TESActorBaseData component.
	// TESActorBaseData::actorData starts at component+0x08, actorBaseFlags is the first uint32.
	// Rather than hardcode fragile offsets, use the CHANGE_TYPES flag pattern:
	// In FO4, the female flag in actorBaseFlags is 0x1.
	// Safe approach: the TESNPC vtable has a GetSex() at a known index, but it's not exposed.
	// For now, return false if we can't safely determine this.
	// TODO: Hook TESNPC::GetSex or find the vtable index for a reliable call.
	return false;
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
	char buf[256]{};
	strncpy_s(buf, editorID.c_str(), sizeof(buf) - 1);
	ImGui::SetNextItemWidth(250);
	if (ImGui::InputText("EditorID##kw", buf, sizeof(buf))) {
		editorID = buf;
		a_dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Resolve##kw")) {
		if (!editorID.empty()) {
			cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
		}
	}
	if (!editorID.empty()) {
		keywordForm.DrawEditWidgets("Keyword Form", a_dirty);
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
	float level = 1.0f;
	// Use base NPC level from ACTOR_BASE_DATA if available
	auto* base = a_refr->data.objectReference;
	if (base && base->GetFormType() == RE::ENUM_FORM_ID::kNPC_) {
		// TESActorBaseData::actorData.level is at a known offset within the NPC form.
		// For player, use the experience-based level from the ActorValue system.
		auto* av = RE::ActorValue::GetSingleton();
		if (av && av->experience) {
			level = actor->GetActorValue(*av->experience);
			// Experience AV stores XP, not level. Use base data level instead.
		}
	}
	// Fallback: read level from ACTOR_BASE_DATA stored in the base form.
	// ACTOR_BASE_DATA::level is a uint16 at offset +0x06 within the struct.
	// For a robust approach, just query the "Level" form variable if available.
	if (auto* av = RE::ActorValue::GetSingleton()) {
		// There's no "level" ActorValueInfo in the singleton, so use base data.
		// The NPC base has actorData.level but we can't safely access it without TESNPC.
		// Player level is typically derived. For now use 1.0 as fallback for non-player.
		auto* player = a_refr->As<RE::PlayerCharacter>();
		if (player) {
			// Player level can be approximated from game data
			// PlayerCharacter has no direct GetLevel, but the Papyrus function exists
			// For now, best effort: player level starts at 1
			level = 1.0f;
		}
	}
	float compareVal = numericValue.GetValue(a_refr);
	return CompareValues(level, comparison, compareVal);
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
		static int s_ammoLogCount = 0;
		if (s_ammoLogCount < 20 || (s_ammoLogCount % 500 == 0)) {
			logger::info("[OAR-Cond] CurrentMagazineAmmo: count={} {} {} -> {}",
				ammoCount, ComparisonOpToString(comparison), compareVal, result);
		}
		s_ammoLogCount++;
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
	char buf[256]{};
	strncpy_s(buf, editorID.c_str(), sizeof(buf) - 1);
	ImGui::SetNextItemWidth(250);
	if (ImGui::InputText("EditorID##eqkw", buf, sizeof(buf))) {
		editorID = buf;
		a_dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Resolve##eqkw")) {
		if (!editorID.empty()) {
			cachedKeyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(editorID);
		}
	}
	if (!editorID.empty()) {
		keywordForm.DrawEditWidgets("Keyword Form", a_dirty);
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
