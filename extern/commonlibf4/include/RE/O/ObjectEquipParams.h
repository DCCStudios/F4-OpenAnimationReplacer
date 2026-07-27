#pragma once

namespace RE
{
	class __declspec(novtable) ObjectEquipParams
	{
	public:
		// members
		std::uint32_t       stackID;            // 00
		std::uint32_t       number;             // 04
		const BGSEquipSlot* equipSlot;          // 08
		const BGSEquipSlot* slotBeingReplaced;  // 10
		bool                queueEquip;         // 18
		bool                forceEquip;         // 19
		bool                playEquipSounds;    // 1A
		bool                applyNow;           // 1B
		bool                locked;             // 1C
		bool                extraWasDeleted;    // 1D
	};
	static_assert(sizeof(ObjectEquipParams) == 0x20);
}
