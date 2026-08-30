#include "Functions.h"
#include "Conditions.h"
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
	OAR_VLOG("[OAR-Func] SetGraphVariable '{}' on {:X} (value pending implementation)", variableName, a_refr->GetFormID());
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

	// Play through BSAudioManager by EditorID name. Uses the same proven
	// GetSoundHandleByName/FadeInPlay path as the annotation audio in
	// Hooks.cpp (the old parallel IDs 1168512/57416/1492470 have no NG/AE
	// Address Library entries). Multi-runtime: { OG, NG }; the AE databases
	// carry the same NG ids, so the NG slot pads forward.
	static REL::Relocation<void**> s_audioMgrPtr{ REL::ID({ 1321158, 2703058 }) };
	void* audioMgr = *s_audioMgrPtr;
	if (!audioMgr) return;

	struct SoundHandle
	{
		uint32_t soundID{ 0 };
		bool assumeSuccess{ false };
		int8_t state{ 0 };
	};
	static_assert(sizeof(SoundHandle) == 0x8);

	using GetSoundByName_t = void(*)(void* mgr, SoundHandle* handle, const char* name,
		float distance, uint32_t usageFlags, void* extraData);
	static REL::Relocation<GetSoundByName_t> GetSoundByName{ REL::ID({ 196484, 2267104 }) };

	using FadeInPlay_t = bool(*)(SoundHandle* handle, uint16_t ms);
	static REL::Relocation<FadeInPlay_t> FadeInPlay{ REL::ID({ 353528, 2267075 }) };

	SoundHandle handle{};
	GetSoundByName(audioMgr, &handle, soundName.c_str(), 0.f, 0x1A, nullptr);
	if (handle.soundID == 0 && !handle.assumeSuccess) return;
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
	OAR_VLOG("[OAR-Func] UnequipSlot {} on {:X}", slotIndex, a_refr->GetFormID());
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
