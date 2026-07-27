#pragma once

#include "RE/T/TESForm.h"

namespace RE
{
	class __declspec(novtable) BGSRelationship :
		public TESForm  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::BGSRelationship };
		static constexpr auto VTABLE{ VTABLE::BGSRelationship };
		static constexpr auto FORM_ID{ ENUM_FORM_ID::kRELA };

		enum class RELATIONSHIP_LEVEL
		{
			kLover = 0x0,
			kAlly = 0x1,
			kConfidant = 0x2,
			kFriend = 0x3,
			kAcquaintance = 0x4,
			kRival = 0x5,
			kFoe = 0x6,
			kEnemy = 0x7,
			kArchnemesis = 0x8,
			kCount = 0x9
		};

		// members
		TESNPC*             npc1;        // 20
		TESNPC*             npc2;        // 28
		BGSAssociationType* assocType;   // 30
		std::uint32_t       packedData;  // 38
	};
	static_assert(sizeof(BGSRelationship) == 0x40);
}
