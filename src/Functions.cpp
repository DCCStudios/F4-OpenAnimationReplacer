#include "Functions.h"
#include "Conditions.h"
#include "RE/Bethesda/ActorValueInfo.h"
#include <random>

// ===== SendAnimEvent =====

void SendAnimEventFunction::Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*)
{
	if (!a_refr || eventName.empty()) return;
	RE::BSFixedString evt(eventName.c_str());
	a_refr->NotifyAnimationGraphImpl(evt);
}

void SendAnimEventFunction::Initialize(const nlohmann::json& a_json)
{
	if (a_json.contains("eventName")) eventName = a_json["eventName"].get<std::string>();
}

// ===== SetGraphVariable =====

void SetGraphVariableFunction::Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*)
{
	if (!a_refr || variableName.empty()) return;
	// IAnimationGraphManagerHolder SetGraphVariable is not directly exposed in this CommonLibF4 version.
	// Access through the graph manager's character's behavior graph variable map.
	RE::BSTSmartPointer<RE::BSAnimationGraphManager> mgr;
	if (!a_refr->GetAnimationGraphManagerImpl(mgr) || !mgr) return;
	// Walk each graph and set the variable via hkbBehaviorGraph API
	RE::BSFixedString varName(variableName.c_str());
	for (std::uint32_t i = 0; i < mgr->graph.size(); ++i) {
		auto& graph = mgr->graph[i];
		if (!graph) continue;
		// The character exposes variable set through hkbBehaviorGraph
		// Use NotifyAnimationGraphImpl as a proxy - the variable API requires relocation
		// For now, use the Impl virtual which IS on IAnimationGraphManagerHolder
	}
	// Fallback: Use the underlying virtual table call
	// IAnimationGraphManagerHolder vtable index for SetGraphVariable* varies
	// For safety, just log
	logger::info("[OAR-Func] SetGraphVariable '{}' on {:X} (value pending implementation)", variableName, a_refr->GetFormID());
}

void SetGraphVariableFunction::Initialize(const nlohmann::json& a_json)
{
	if (a_json.contains("variableName")) variableName = a_json["variableName"].get<std::string>();
	if (a_json.contains("type")) {
		std::string typeStr = a_json["type"].get<std::string>();
		if (typeStr == "Bool") varType = VarType::kBool;
		else if (typeStr == "Int") varType = VarType::kInt;
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

// ===== ModActorValue =====

void ModActorValueFunction::Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*)
{
	if (!a_refr) return;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor) return;

	if (!resolved) {
		resolved = true;
		if (!actorValueName.empty()) {
			cachedAVInfo = RE::TESForm::GetFormByEditorID<RE::ActorValueInfo>(actorValueName);
		}
	}
	if (!cachedAVInfo) return;

	float current = actor->GetActorValue(*cachedAVInfo);
	actor->SetActorValue(*cachedAVInfo, current + amount);
}

void ModActorValueFunction::Initialize(const nlohmann::json& a_json)
{
	if (a_json.contains("actorValue")) actorValueName = a_json["actorValue"].get<std::string>();
	if (a_json.contains("amount")) amount = a_json["amount"].get<float>();
}

// ===== PlaySound =====

void PlaySoundFunction::Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*)
{
	if (!a_refr || soundName.empty()) return;
	// Use BSAudioManager to play the sound descriptor
	using GetSoundHandle_t = bool(*)(void*, void*, uint32_t, const char*);
	using FadeInPlay_t = bool(*)(void*, int);

	static auto* audioMgr = []() -> void* {
		REL::Relocation<void**> singleton{ REL::ID(1168512) };
		return *singleton;
	}();

	if (!audioMgr) return;

	struct SoundHandle { uint32_t soundID{ 0xFFFFFFFF }; uint32_t unk04{ 0 }; };
	SoundHandle handle{};

	REL::Relocation<GetSoundHandle_t> GetSoundHandleByName{ REL::ID(57416) };
	REL::Relocation<FadeInPlay_t> FadeInPlay{ REL::ID(1492470) };

	if (!GetSoundHandleByName(audioMgr, &handle, 0, soundName.c_str())) return;
	if (handle.soundID == 0xFFFFFFFF) return;
	FadeInPlay(&handle, 0);
}

void PlaySoundFunction::Initialize(const nlohmann::json& a_json)
{
	if (a_json.contains("soundName")) soundName = a_json["soundName"].get<std::string>();
}

// ===== UnequipSlot =====

void UnequipSlotFunction::Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*)
{
	if (!a_refr) return;
	auto* actor = a_refr->As<RE::Actor>();
	if (!actor || !actor->currentProcess || !actor->currentProcess->middleHigh) return;
	// Unequip by biped slot index - requires ActorEquipManager
	// For now, log intent but don't crash
	logger::info("[OAR-Func] UnequipSlot {} on {:X}", slotIndex, a_refr->GetFormID());
}

void UnequipSlotFunction::Initialize(const nlohmann::json& a_json)
{
	if (a_json.contains("slotIndex")) slotIndex = a_json["slotIndex"].get<int32_t>();
}

// ===== CONDITION meta-function =====

void ConditionFunction::Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen)
{
	bool pass = conditionSet ? conditionSet->EvaluateAll(a_refr, a_clipGen, nullptr) : true;
	auto& funcs = pass ? trueFunctions : falseFunctions;
	for (auto& f : funcs) {
		if (f) f->Execute(a_refr, a_clipGen);
	}
}

void ConditionFunction::Initialize(const nlohmann::json& a_json)
{
	if (a_json.contains("conditions") && a_json["conditions"].is_array()) {
		conditionSet = std::make_unique<ConditionSet>();
		for (const auto& cj : a_json["conditions"]) {
			if (auto c = CreateConditionFromJson(cj)) {
				conditionSet->AddCondition(std::move(c));
			}
		}
	}
	trueFunctions = ParseFunctionArray(a_json, "trueFunctions");
	falseFunctions = ParseFunctionArray(a_json, "falseFunctions");
}

// ===== RANDOM meta-function =====

void RandomFunction::Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen)
{
	static thread_local std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> dist(0.f, 1.f);
	if (dist(rng) < chance) {
		for (auto& f : functions) {
			if (f) f->Execute(a_refr, a_clipGen);
		}
	}
}

void RandomFunction::Initialize(const nlohmann::json& a_json)
{
	if (a_json.contains("chance")) chance = a_json["chance"].get<float>();
	functions = ParseFunctionArray(a_json, "functions");
}

// ===== ONE meta-function =====

void OneFunction::Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen)
{
	if (fired) return;
	fired = true;
	for (auto& f : functions) {
		if (f) f->Execute(a_refr, a_clipGen);
	}
}

void OneFunction::Initialize(const nlohmann::json& a_json)
{
	functions = ParseFunctionArray(a_json, "functions");
}

// ===== Registration =====

void RegisterAllFunctions()
{
	auto* factory = FunctionFactory::GetSingleton();
	factory->Register("SendAnimEvent", [] { return std::make_unique<SendAnimEventFunction>(); });
	factory->Register("SetGraphVariable", [] { return std::make_unique<SetGraphVariableFunction>(); });
	factory->Register("ModActorValue", [] { return std::make_unique<ModActorValueFunction>(); });
	factory->Register("PlaySound", [] { return std::make_unique<PlaySoundFunction>(); });
	factory->Register("UnequipSlot", [] { return std::make_unique<UnequipSlotFunction>(); });
	factory->Register("CONDITION", [] { return std::make_unique<ConditionFunction>(); });
	factory->Register("RANDOM", [] { return std::make_unique<RandomFunction>(); });
	factory->Register("ONE", [] { return std::make_unique<OneFunction>(); });
	factory->Register("CastSpell", [] { return std::make_unique<CastSpellFunction>(); });
	factory->Register("DispelSpell", [] { return std::make_unique<DispelSpellFunction>(); });
	factory->Register("SpawnParticle", [] { return std::make_unique<SpawnParticleFunction>(); });

	logger::info("[OAR] Registered {} function types", factory->GetAllFactories().size());
}

std::unique_ptr<IFunction> CreateFunctionFromJson(const nlohmann::json& a_json)
{
	if (!a_json.contains("function")) return nullptr;
	std::string funcName = a_json["function"].get<std::string>();
	auto func = FunctionFactory::GetSingleton()->Create(funcName);
	if (!func) return nullptr;
	try {
		func->Initialize(a_json);
	} catch (const std::exception& e) {
		logger::error("[OAR-Func] Failed to initialize '{}': {}", funcName, e.what());
		return nullptr;
	}
	return func;
}

std::vector<std::unique_ptr<IFunction>> ParseFunctionArray(const nlohmann::json& a_json, const std::string& a_key)
{
	std::vector<std::unique_ptr<IFunction>> result;
	if (a_json.contains(a_key) && a_json[a_key].is_array()) {
		for (const auto& fj : a_json[a_key]) {
			if (auto f = CreateFunctionFromJson(fj)) {
				result.push_back(std::move(f));
			}
		}
	}
	return result;
}
