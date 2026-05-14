#pragma once

#include "HavokTypes.h"

void RegisterActorCharacter(RE::TESObjectREFR* a_refr);
void ClearCharacterCache();
void SetGameFullyLoaded(bool a_loaded);
void SetHasActiveReplacements(bool a_has);
bool HasActiveReplacements();

namespace Hooks
{
	void Install();

	namespace ClipGeneratorHooks
	{
		void Install();

		using ActivateFn = void(*)(RE::hkbClipGenerator*, const RE::hkbContext*);
		using UpdateFn = void(*)(RE::hkbClipGenerator*, const RE::hkbContext*, float);
		using DeactivateFn = void(*)(RE::hkbClipGenerator*, const RE::hkbContext*);
		using GenerateFn = void(*)(RE::hkbClipGenerator*, const RE::hkbContext*, const RE::hkbGeneratorOutput**, RE::hkbGeneratorOutput&, float);
		using StartEchoFn = void(*)(RE::hkbClipGenerator*, float);

		inline ActivateFn  _Activate{ nullptr };
		inline UpdateFn    _Update{ nullptr };
		inline DeactivateFn _Deactivate{ nullptr };
		inline GenerateFn  _Generate{ nullptr };
		inline StartEchoFn _StartEcho{ nullptr };
	}

	namespace LoadClipsHooks
	{
		void Install();
		bool TryDeferredInjection();

		using LoadClipsFn = void(*)(RE::hkbCharacterStringData*, void*, void*, RE::hkbBehaviorGraph*, const char*, void*);
		inline LoadClipsFn _LoadClips{ nullptr };
		inline LoadClipsFn _LoadClips2{ nullptr };
		inline bool bHookInstalled{ false };
	}

	namespace EnginePatchHooks
	{
		void Install();
	}

	namespace PreloadHooks
	{
		void Install();
		void PreloadReplacementAnimations(RE::BShkbAnimationGraph* a_graph);
	}

	namespace UpdateHooks
	{
		void Install();

		using RunActorUpdatesFn = void(*)();
		inline RunActorUpdatesFn RunActorUpdatesOrig{ nullptr };
	}

	namespace FileRedirectHooks
	{
		void Install();
		void BuildFileRedirectMap();
	}
}
