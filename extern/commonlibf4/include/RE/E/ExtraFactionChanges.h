#pragma once

#include "RE/B/BSExtraData.h"
#include "RE/F/FACTION_RANK.h"

namespace RE
{
	class __declspec(novtable) ExtraFactionChanges :
		public BSExtraData  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::ExtraFactionChanges };
		static constexpr auto VTABLE{ VTABLE::ExtraFactionChanges };
		static constexpr auto TYPE{ EXTRA_DATA_TYPE::kFactionChanges };

		// members
		bool                                          removeCrimeFaction;  // 18
		BSTArray<FACTION_RANK, BSTArrayHeapAllocator> factionChanges;      // 20
		TESFaction*                                   crimeFaction;        // 38
	};
	static_assert(sizeof(ExtraFactionChanges) == 0x40);
}
