#include "Conditions.h"
#include "Utils.h"
#include <imgui.h>

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
	// TODO: TESNPC is only forward-declared in CommonLibF4 FO4.
	// Need to reverse-engineer race access in Phase 0 / Phase 2.
	return false;
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
	// TODO: TESNPC is only forward-declared in CommonLibF4 FO4.
	// Need to reverse-engineer sex flag access in Phase 0 / Phase 2.
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
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	// ActorState::moveMode bit 8 = sprinting
	return (actor->moveMode & 0x100) != 0;
}

bool IsInAirCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	// ActorState::flyState > 0 means in air
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
	// TODO: FO4 uses ActorValueInfo, not ActorValue enum for level lookup.
	// Needs proper ActorValueInfo resolution in Phase 2.
	float level = 1.0f;
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
	if (!actor) return false;
	// equippedItems is in MiddleHighProcessData, accessed through currentProcess
	// TODO: implement proper equipped item walk through AIProcess in Phase 2
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
	if (!actor) return false;
	// Power armor: biped slot 40 (0-indexed) populated check
	// Biped slot 40 = INTV_PowerArmor in FO4
	// actor->biped is BSTSmartPointer<BipedAnim>
	// TODO: proper BipedAnim layout needed — stub for now
	return false;
}

bool IsSneakingCondition::EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, const SubMod*) const
{
	if (!a_refr) return false;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return false;
	// TODO: Needs runtime verification of the exact moveMode sneak bits in FO4.
	// forceSneak is set by script/console but correlates with actual sneak state.
	return actor->forceSneak != 0;
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
	// gunState == 6 means aiming down sights in FO4
	return actor->gunState == 6;
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
	// TODO: need ActorValueInfo lookup by name — stub for now
	float compareVal = numericValue.GetValue(a_refr);
	return CompareValues(0.0f, comparison, compareVal);
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
