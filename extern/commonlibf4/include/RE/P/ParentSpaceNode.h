#pragma once

namespace RE
{
	struct ParentSpaceNode
	{
	public:
		// members
		bool           worldspace;    // 00
		TESWorldSpace* worldSpace;    // 08
		TESObjectCELL* interiorCell;  // 10
	};
	static_assert(sizeof(ParentSpaceNode) == 0x18);
}
