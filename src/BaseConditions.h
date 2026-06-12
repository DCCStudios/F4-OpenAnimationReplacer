#pragma once

#include "HavokTypes.h"

class SubMod;

enum class ComparisonOperator : int32_t
{
	kEqual = 0,
	kNotEqual,
	kGreater,
	kGreaterEqual,
	kLess,
	kLessEqual,
};

inline bool CompareValues(float a_lhs, ComparisonOperator a_op, float a_rhs)
{
	switch (a_op) {
	case ComparisonOperator::kEqual:        return std::abs(a_lhs - a_rhs) < 0.0001f;
	case ComparisonOperator::kNotEqual:     return std::abs(a_lhs - a_rhs) >= 0.0001f;
	case ComparisonOperator::kGreater:      return a_lhs > a_rhs;
	case ComparisonOperator::kGreaterEqual: return a_lhs >= a_rhs;
	case ComparisonOperator::kLess:         return a_lhs < a_rhs;
	case ComparisonOperator::kLessEqual:    return a_lhs <= a_rhs;
	default: return false;
	}
}

inline const char* ComparisonOperatorToString(ComparisonOperator a_op)
{
	switch (a_op) {
	case ComparisonOperator::kEqual:        return "==";
	case ComparisonOperator::kNotEqual:     return "!=";
	case ComparisonOperator::kGreater:      return ">";
	case ComparisonOperator::kGreaterEqual: return ">=";
	case ComparisonOperator::kLess:         return "<";
	case ComparisonOperator::kLessEqual:    return "<=";
	default: return "?";
	}
}

class ICondition
{
public:
	virtual ~ICondition() = default;

	virtual bool Evaluate(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_parentSubMod) const = 0;
	virtual void Initialize(const nlohmann::json& a_json) = 0;
	virtual void Serialize(nlohmann::json& a_json) const = 0;
	virtual std::string GetName() const = 0;
	virtual std::string GetDescription() const = 0;
	virtual std::string GetParameterString() const { return ""; }
	virtual void DrawEditWidgets(bool& a_dirty) { (void)a_dirty; }
	virtual bool IsStub() const { return false; }
	virtual std::string GetStubReason() const { return ""; }

	bool IsDisabled() const { return disabled; }
	void SetDisabled(bool a_val) { disabled = a_val; }

	bool IsNegated() const { return negated; }
	void SetNegated(bool a_val) { negated = a_val; }

	mutable std::optional<bool> lastEvalResult;

protected:
	bool disabled{ false };
	bool negated{ false };
};

class ConditionBase : public ICondition
{
public:
	bool Evaluate(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_parentSubMod) const override final
	{
		if (disabled) {
			lastEvalResult = true;
			return true;
		}

		bool result = EvaluateImpl(a_refr, a_clipGen, a_parentSubMod);
		if (negated) result = !result;
		lastEvalResult = result;
		return result;
	}

	void Initialize(const nlohmann::json& a_json) override
	{
		if (a_json.contains("negated")) negated = a_json["negated"].get<bool>();
		if (a_json.contains("disabled")) disabled = a_json["disabled"].get<bool>();
		InitializeImpl(a_json);
	}

	void Serialize(nlohmann::json& a_json) const override
	{
		a_json["condition"] = GetName();
		a_json["negated"] = negated;
		a_json["disabled"] = disabled;
		SerializeImpl(a_json);
	}

protected:
	virtual bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_parentSubMod) const = 0;
	virtual void InitializeImpl(const nlohmann::json& a_json) = 0;
	virtual void SerializeImpl(nlohmann::json& a_json) const = 0;
};

class ConditionSet
{
public:
	bool EvaluateAll(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_parentSubMod) const
	{
		for (const auto& condition : conditions) {
			if (!condition->Evaluate(a_refr, a_clipGen, a_parentSubMod))
				return false;
		}
		return true;
	}

	bool EvaluateAny(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGen, const SubMod* a_parentSubMod) const
	{
		if (conditions.empty()) return true;
		for (const auto& condition : conditions) {
			if (condition->Evaluate(a_refr, a_clipGen, a_parentSubMod))
				return true;
		}
		return false;
	}

	void AddCondition(std::unique_ptr<ICondition> a_condition) { conditions.push_back(std::move(a_condition)); }
	void ClearConditions() { conditions.clear(); }
	void RemoveCondition(size_t a_index) {
		if (a_index < conditions.size()) conditions.erase(conditions.begin() + a_index);
	}

	const std::vector<std::unique_ptr<ICondition>>& GetConditions() const { return conditions; }
	std::vector<std::unique_ptr<ICondition>>& GetConditionsMutable() { return conditions; }
	bool IsEmpty() const { return conditions.empty(); }

private:
	std::vector<std::unique_ptr<ICondition>> conditions;
};

class ConditionFactory
{
public:
	using FactoryFn = std::function<std::unique_ptr<ICondition>()>;

	static ConditionFactory* GetSingleton()
	{
		static ConditionFactory singleton;
		return &singleton;
	}

	void Register(const std::string& a_name, FactoryFn a_factory)
	{
		factories[a_name] = std::move(a_factory);
	}

	std::unique_ptr<ICondition> Create(const std::string& a_name) const
	{
		auto it = factories.find(a_name);
		if (it != factories.end()) {
			return it->second();
		}
		logger::warn("[OAR] Unknown condition type: '{}'", a_name);
		return nullptr;
	}

	const std::unordered_map<std::string, FactoryFn>& GetAllFactories() const { return factories; }

private:
	ConditionFactory() = default;
	std::unordered_map<std::string, FactoryFn> factories;
};
