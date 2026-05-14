#pragma once

// C-compatible API for third-party F4SE plugins to register custom conditions
// Usage: call RequestPluginAPI_Conditions() from your F4SEPlugin_Load to get the API

#include <cstdint>

#ifdef OAR_EXPORTS
#define OAR_API __declspec(dllexport)
#else
#define OAR_API __declspec(dllimport)
#endif

namespace OAR_API_NS
{
	constexpr uint32_t kConditionsAPIVersion = 1;
	constexpr uint32_t kAnimationsAPIVersion = 1;

	using EvaluateFunc = bool(*)(void* a_refr, void* a_clipGen, void* a_parentSubMod);
	using InitializeFunc = void(*)(const char* a_jsonStr);
	using SerializeFunc = const char*(*)(void);
	using GetNameFunc = const char*(*)(void);

	struct CustomConditionDescriptor
	{
		const char* typeName;
		GetNameFunc getDisplayName;
		GetNameFunc getDescription;
		EvaluateFunc evaluate;
		InitializeFunc initialize;
		SerializeFunc serialize;
	};

	struct IConditionRegistrationAPI
	{
		uint32_t apiVersion;
		bool (*RegisterCustomCondition)(const CustomConditionDescriptor* a_descriptor);
		bool (*UnregisterCustomCondition)(const char* a_typeName);
		uint32_t (*GetRegisteredConditionCount)(void);
	};

	struct IAnimationRegistrationAPI
	{
		uint32_t apiVersion;
		bool (*AddReplacementPath)(const char* a_originalPath, const char* a_replacementPath,
			int32_t a_priority, const char* a_modName);
		bool (*RemoveReplacementPath)(const char* a_originalPath, const char* a_replacementPath);
	};
}

extern "C" OAR_API OAR_API_NS::IConditionRegistrationAPI* RequestPluginAPI_Conditions();
extern "C" OAR_API OAR_API_NS::IAnimationRegistrationAPI* RequestPluginAPI_Animations();
