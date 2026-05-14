#pragma once

#include "HavokTypes.h"

namespace HavokGraphUtils
{
	struct ActiveClipInfo
	{
		RE::hkbClipGenerator* clipGenerator{ nullptr };
		RE::hkbNode* node{ nullptr };
		uint16_t nodeId{ 0 };
	};

	struct AnnotationEntry
	{
		float time{ 0.f };
		std::string text;
	};

	void EnumerateActiveClips(
		RE::hkbBehaviorGraph* a_graph,
		std::vector<ActiveClipInfo>& a_outClips);

	std::vector<AnnotationEntry> ReadAnnotations(
		RE::hkbClipGenerator* a_clipGen);

	void OverwriteLocalTime(
		RE::hkbClipGenerator* a_clipGen,
		float a_newTime);

	int32_t GetCurrentStateMachineState(
		RE::hkbBehaviorGraph* a_graph);

	RE::hkbCharacter* GetCharacterFromGraph(
		RE::BShkbAnimationGraph* a_graph);

	RE::hkbBehaviorGraph* GetBehaviorGraphForActor(
		RE::Actor* a_actor);
}
