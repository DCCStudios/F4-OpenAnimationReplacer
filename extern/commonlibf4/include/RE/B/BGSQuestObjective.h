#pragma once

#include "RE/B/BGSLocalizedString.h"

namespace RE
{
	class TESQuest;
	class TESQuestTarget;

	class BGSQuestObjective
	{
	public:
		// members
		BGSLocalizedString displayText;  // 00
		TESQuest*          ownerQuest;   // 08
		TESQuestTarget**   targets;      // 10
		std::uint32_t      numTargets;   // 18
		std::uint16_t      index;        // 1C
		bool               initialized;  // 1E
		char               state;        // 1F
		std::uint32_t      flags;        // 20
	};
	static_assert(sizeof(BGSQuestObjective) == 0x28);
}
