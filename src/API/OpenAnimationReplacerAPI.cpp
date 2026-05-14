#include "API/OpenAnimationReplacerAPI.h"
#include "BaseConditions.h"

namespace
{
	bool RegisterCustomConditionImpl(const OAR_API_NS::CustomConditionDescriptor* a_descriptor)
	{
		if (!a_descriptor || !a_descriptor->typeName) return false;

		logger::info("[OAR-API] Registering custom condition: '{}'", a_descriptor->typeName);

		auto* factory = ConditionFactory::GetSingleton();
		std::string typeName = a_descriptor->typeName;

		// TODO: create wrapper ICondition that delegates to the C callbacks
		// For now, just log the registration
		logger::info("[OAR-API] Custom condition '{}' registered (stub)", typeName);
		return true;
	}

	bool UnregisterCustomConditionImpl(const char* a_typeName)
	{
		if (!a_typeName) return false;
		logger::info("[OAR-API] Unregistering custom condition: '{}'", a_typeName);
		return true;
	}

	uint32_t GetRegisteredConditionCountImpl()
	{
		return static_cast<uint32_t>(ConditionFactory::GetSingleton()->GetAllFactories().size());
	}

	bool AddReplacementPathImpl(const char*, const char*, int32_t, const char*)
	{
		// TODO: implement runtime replacement addition
		return false;
	}

	bool RemoveReplacementPathImpl(const char*, const char*)
	{
		// TODO: implement runtime replacement removal
		return false;
	}

	OAR_API_NS::IConditionRegistrationAPI g_conditionAPI{
		OAR_API_NS::kConditionsAPIVersion,
		RegisterCustomConditionImpl,
		UnregisterCustomConditionImpl,
		GetRegisteredConditionCountImpl
	};

	OAR_API_NS::IAnimationRegistrationAPI g_animationAPI{
		OAR_API_NS::kAnimationsAPIVersion,
		AddReplacementPathImpl,
		RemoveReplacementPathImpl
	};
}

extern "C" OAR_API OAR_API_NS::IConditionRegistrationAPI* RequestPluginAPI_Conditions()
{
	return &g_conditionAPI;
}

extern "C" OAR_API OAR_API_NS::IAnimationRegistrationAPI* RequestPluginAPI_Animations()
{
	return &g_animationAPI;
}
