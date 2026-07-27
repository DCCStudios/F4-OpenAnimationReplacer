#pragma once

namespace RE
{
	class BSLight;
	class BSPortal;

	class __declspec(novtable) BSPortalSharedNode :
		public NiNode  // 00
	{
	public:
		inline static constexpr auto RTTI{ RTTI::BSPortalSharedNode };
		inline static constexpr auto VTABLE{ VTABLE::BSPortalSharedNode };
		inline static constexpr auto Ni_RTTI{ Ni_RTTI::BSPortalSharedNode };

		// members
		BSTArray<BSLight*, BSTArrayHeapAllocator> lights;  // 140
		BSPortal*                                 portal;  // 158
	};
	static_assert(sizeof(BSPortalSharedNode) == 0x160);
}
