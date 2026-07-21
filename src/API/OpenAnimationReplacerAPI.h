#pragma once

// =============================================================================
// Open Animation Replacer F4 — DLL Export Header (internal)
// =============================================================================
// This header defines the DLL exports. Plugin authors do NOT include this file;
// they use the redistributable OpenAnimationReplacerAPI-Conditions.h instead.
// =============================================================================

#ifdef OAR_EXPORTS
#define OAR_API __declspec(dllexport)
#else
#define OAR_API __declspec(dllimport)
#endif

// The exported function returns a pointer to the IConditionsAPI implementation.
// Plugin authors access it via OAR::Conditions::GetAPI() in the SDK header.
extern "C" OAR_API void* RequestPluginAPI_Conditions();

// Returns a pointer to the IClipsAPI implementation (clip/replacement queries).
// Plugin authors access it via OAR::Clips::GetAPI() in the SDK header
// (OpenAnimationReplacerAPI-Clips.h).
extern "C" OAR_API void* RequestPluginAPI_Clips();
