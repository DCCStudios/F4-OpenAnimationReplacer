#pragma once

#include "RE/B/BGSBaseAlias.h"

namespace RE
{
	class BGSKeyword;
	class BGSLocation;
	class TESCondition;
	class TESQuest;

	class __declspec(novtable) BGSLocAlias :
		public BGSBaseAlias  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::BGSLocAlias };
		static constexpr auto VTABLE{ VTABLE::BGSLocAlias };

		// members
		BGSLocation*  forcedLoc;              // 28
		BGSKeyword*   forcedFromAliasFilter;  // 30
		TESQuest*     externalQuest;          // 38
		TESCondition* conditions;             // 40
		std::uint32_t forcedFromAlias;        // 48
		std::uint32_t forcedFromEvent;        // 4C
		std::uint32_t forcedEventData;        // 50
		std::uint32_t externalAlias;          // 54
		std::uint32_t closestToAlias;         // 58
	};
	static_assert(sizeof(BGSLocAlias) == 0x60);
}
