#pragma once

// =============================================================================
// Open Animation Replacer F4 — Redistributable API Header
// =============================================================================
// Plugin authors: Copy this file + OpenAnimationReplacer-ConditionTypes.h into
// your project. Then call OAR::Conditions::GetAPI() in kPostLoad to register
// your custom conditions.
//
// Minimal example:
//
//   #include "OAR/OpenAnimationReplacerAPI-Conditions.h"
//
//   class MyCondition : public OAR::ConditionBase { ... };
//
//   void OnPostLoad() {
//       if (auto* api = OAR::Conditions::GetAPI()) {
//           api->RegisterCondition("MyCondition",
//               [] { return std::make_unique<MyCondition>(); });
//       }
//   }
//
// =============================================================================

#include "OpenAnimationReplacer-ConditionTypes.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace OAR::Conditions
{
	// API version — bump on breaking changes
	inline constexpr uint32_t kAPIVersion = 2;

	// Result codes returned by API registration methods
	enum class APIResult : uint8_t
	{
		OK,               // Condition registered successfully
		AlreadyRegistered,// A condition with that name already exists
		Invalid,          // Arguments were null or malformed
		Failed            // Internal failure
	};

	// Factory function type that creates a new condition instance
	using ConditionFactoryFn = std::unique_ptr<OAR::ICondition>(*)();

	// =========================================================================
	// IConditionsAPI — virtual interface returned by RequestPluginAPI_Conditions
	// =========================================================================

	class IConditionsAPI
	{
	public:
		virtual ~IConditionsAPI() = default;

		// Returns the API version implemented by this OAR build
		virtual uint32_t GetAPIVersion() const = 0;

		// Register a custom condition type. Call during kPostLoad or kPostPostLoad.
		// a_name: unique identifier for JSON serialization (e.g. "MyDistance")
		// a_factory: function that returns a std::make_unique<YourCondition>()
		virtual APIResult RegisterCondition(const char* a_name, ConditionFactoryFn a_factory) = 0;

		// Unregister a previously registered custom condition
		virtual bool UnregisterCondition(const char* a_name) = 0;

		// Total number of registered condition types (built-in + custom)
		virtual uint32_t GetRegisteredConditionCount() const = 0;
	};

	// =========================================================================
	// GetAPI() — plugin authors call this to get the API interface
	// =========================================================================
	// Call in your kPostLoad messaging handler. Returns nullptr if OAR is not
	// installed or the DLL has not loaded yet.

	inline IConditionsAPI* GetAPI()
	{
		HMODULE handle = GetModuleHandleA("OpenAnimationReplacer.dll");
		if (!handle) return nullptr;

		using RequestFn = IConditionsAPI * (*)();
		auto fn = reinterpret_cast<RequestFn>(GetProcAddress(handle, "RequestPluginAPI_Conditions"));
		return fn ? fn() : nullptr;
	}

} // namespace OAR::Conditions
