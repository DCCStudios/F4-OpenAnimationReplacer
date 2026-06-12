#include "API/OpenAnimationReplacerAPI.h"
#include "BaseConditions.h"

namespace
{
	// =========================================================================
	// API Result codes (mirrors OAR::Conditions::APIResult in the SDK header)
	// =========================================================================
	enum class APIResult : uint8_t
	{
		OK,
		AlreadyRegistered,
		Invalid,
		Failed
	};

	// Factory function type matching the SDK header's ConditionFactoryFn
	using ConditionFactoryFn = std::unique_ptr<ICondition>(*)();

	// =========================================================================
	// IConditionsAPI — virtual interface matching the SDK header
	// =========================================================================
	// The vtable layout here MUST match OAR::Conditions::IConditionsAPI exactly.
	// External plugins cast the void* return from RequestPluginAPI_Conditions
	// to their SDK header's IConditionsAPI* and call through the vtable.

	class IConditionsAPIInternal
	{
	public:
		virtual ~IConditionsAPIInternal() = default;
		virtual uint32_t GetAPIVersion() const = 0;
		virtual APIResult RegisterCondition(const char* a_name, ConditionFactoryFn a_factory) = 0;
		virtual bool UnregisterCondition(const char* a_name) = 0;
		virtual uint32_t GetRegisteredConditionCount() const = 0;
	};

	// =========================================================================
	// ConditionsAPIImpl — the actual implementation
	// =========================================================================

	class ConditionsAPIImpl : public IConditionsAPIInternal
	{
	public:
		uint32_t GetAPIVersion() const override
		{
			return 2;
		}

		APIResult RegisterCondition(const char* a_name, ConditionFactoryFn a_factory) override
		{
			if (!a_name || !a_factory) {
				logger::error("[OAR-API] RegisterCondition called with null arguments");
				return APIResult::Invalid;
			}

			auto* factory = ConditionFactory::GetSingleton();
			std::string name(a_name);

			if (factory->GetAllFactories().count(name)) {
				logger::warn("[OAR-API] Condition '{}' already registered", name);
				return APIResult::AlreadyRegistered;
			}

			// The external plugin's factory returns std::unique_ptr<OAR::ICondition>.
			// Since OAR::ICondition (in the SDK header) is binary-identical to our
			// internal ICondition (same vtable layout, same MSVC CRT), we can safely
			// store and call the factory directly.
			factory->Register(name, [a_factory]() -> std::unique_ptr<ICondition> {
				return a_factory();
			});

			logger::info("[OAR-API] Registered custom condition: '{}'", name);
			return APIResult::OK;
		}

		bool UnregisterCondition(const char* a_name) override
		{
			if (!a_name) return false;

			auto* factory = ConditionFactory::GetSingleton();
			std::string name(a_name);

			auto& factories = const_cast<std::unordered_map<std::string, ConditionFactory::FactoryFn>&>(
				factory->GetAllFactories());
			auto it = factories.find(name);
			if (it == factories.end()) {
				logger::warn("[OAR-API] Cannot unregister '{}': not found", name);
				return false;
			}

			factories.erase(it);
			logger::info("[OAR-API] Unregistered condition: '{}'", name);
			return true;
		}

		uint32_t GetRegisteredConditionCount() const override
		{
			return static_cast<uint32_t>(ConditionFactory::GetSingleton()->GetAllFactories().size());
		}
	};

	static ConditionsAPIImpl g_conditionsAPI;
}

// =============================================================================
// DLL export — external plugins call GetProcAddress for this symbol
// =============================================================================

extern "C" OAR_API void* RequestPluginAPI_Conditions()
{
	logger::info("[OAR-API] Conditions API requested (version 2)");
	return static_cast<IConditionsAPIInternal*>(&g_conditionsAPI);
}
