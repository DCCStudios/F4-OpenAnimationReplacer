#pragma once

#include "RE/N/NiNode.h"

namespace RE
{
	class __declspec(novtable) BSNiNode :
		public NiNode  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::BSNiNode };
		static constexpr auto VTABLE{ VTABLE::BSNiNode };
		static constexpr auto Ni_RTTI{ Ni_RTTI::BSNiNode };

		// empty
	};
	static_assert(sizeof(BSNiNode) == 0x140);
}
