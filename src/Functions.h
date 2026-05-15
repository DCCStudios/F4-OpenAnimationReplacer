#pragma once

#include "BaseConditions.h"

class SubMod;

class IFunction
{
public:
	virtual ~IFunction() = default;
	virtual void Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) = 0;
	virtual std::string GetName() const = 0;
	virtual std::string GetDescription() const { return ""; }
	virtual void Initialize(const nlohmann::json& a_json) = 0;
	virtual bool IsStub() const { return false; }
	virtual std::string GetStubReason() const { return ""; }
};

class FunctionFactory
{
public:
	using FactoryFn = std::function<std::unique_ptr<IFunction>()>;

	static FunctionFactory* GetSingleton()
	{
		static FunctionFactory singleton;
		return &singleton;
	}

	void Register(const std::string& a_name, FactoryFn a_factory)
	{
		factories[a_name] = std::move(a_factory);
	}

	std::unique_ptr<IFunction> Create(const std::string& a_name) const
	{
		auto it = factories.find(a_name);
		if (it != factories.end()) return it->second();
		logger::warn("[OAR-Func] Unknown function type: '{}'", a_name);
		return nullptr;
	}

	const std::unordered_map<std::string, FactoryFn>& GetAllFactories() const { return factories; }

private:
	FunctionFactory() = default;
	std::unordered_map<std::string, FactoryFn> factories;
};

// ===== Concrete Functions =====

class SendAnimEventFunction : public IFunction
{
public:
	std::string GetName() const override { return "SendAnimEvent"; }
	void Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) override;
	void Initialize(const nlohmann::json& a_json) override;
private:
	std::string eventName;
};

class SetGraphVariableFunction : public IFunction
{
public:
	std::string GetName() const override { return "SetGraphVariable"; }
	void Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) override;
	void Initialize(const nlohmann::json& a_json) override;
private:
	std::string variableName;
	enum class VarType { kBool, kInt, kFloat } varType{ VarType::kFloat };
	float floatValue{ 0.f };
	int32_t intValue{ 0 };
	bool boolValue{ false };
};

class ModActorValueFunction : public IFunction
{
public:
	std::string GetName() const override { return "ModActorValue"; }
	void Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) override;
	void Initialize(const nlohmann::json& a_json) override;
private:
	std::string actorValueName;
	float amount{ 0.f };
	mutable RE::ActorValueInfo* cachedAVInfo{ nullptr };
	mutable bool resolved{ false };
};

class PlaySoundFunction : public IFunction
{
public:
	std::string GetName() const override { return "PlaySound"; }
	void Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) override;
	void Initialize(const nlohmann::json& a_json) override;
private:
	std::string soundName;
};

class UnequipSlotFunction : public IFunction
{
public:
	std::string GetName() const override { return "UnequipSlot"; }
	void Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) override;
	void Initialize(const nlohmann::json& a_json) override;
private:
	int32_t slotIndex{ 0 };
};

// ===== Meta Functions =====

class ConditionFunction : public IFunction
{
public:
	std::string GetName() const override { return "CONDITION"; }
	void Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) override;
	void Initialize(const nlohmann::json& a_json) override;
private:
	std::unique_ptr<ConditionSet> conditionSet;
	std::vector<std::unique_ptr<IFunction>> trueFunctions;
	std::vector<std::unique_ptr<IFunction>> falseFunctions;
};

class RandomFunction : public IFunction
{
public:
	std::string GetName() const override { return "RANDOM"; }
	void Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) override;
	void Initialize(const nlohmann::json& a_json) override;
private:
	float chance{ 0.5f };
	std::vector<std::unique_ptr<IFunction>> functions;
};

class OneFunction : public IFunction
{
public:
	std::string GetName() const override { return "ONE"; }
	void Execute(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen) override;
	void Initialize(const nlohmann::json& a_json) override;
private:
	std::vector<std::unique_ptr<IFunction>> functions;
	bool fired{ false };
};

// ===== Stub Functions =====

class CastSpellFunction : public IFunction
{
public:
	std::string GetName() const override { return "CastSpell"; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "MagicCaster API not available in FO4 CommonLibF4"; }
	void Execute(RE::TESObjectREFR*, RE::hkbClipGenerator*) override {}
	void Initialize(const nlohmann::json&) override {}
};

class DispelSpellFunction : public IFunction
{
public:
	std::string GetName() const override { return "DispelSpell"; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "MagicTarget::DispelEffect not exposed in FO4 CommonLibF4"; }
	void Execute(RE::TESObjectREFR*, RE::hkbClipGenerator*) override {}
	void Initialize(const nlohmann::json&) override {}
};

class SpawnParticleFunction : public IFunction
{
public:
	std::string GetName() const override { return "SpawnParticle"; }
	bool IsStub() const override { return true; }
	std::string GetStubReason() const override { return "BSTempEffectParticle creation not available in FO4 CommonLibF4"; }
	void Execute(RE::TESObjectREFR*, RE::hkbClipGenerator*) override {}
	void Initialize(const nlohmann::json&) override {}
};

// ===== Registration + Parsing =====

void RegisterAllFunctions();
std::unique_ptr<IFunction> CreateFunctionFromJson(const nlohmann::json& a_json);
std::vector<std::unique_ptr<IFunction>> ParseFunctionArray(const nlohmann::json& a_json, const std::string& a_key);
