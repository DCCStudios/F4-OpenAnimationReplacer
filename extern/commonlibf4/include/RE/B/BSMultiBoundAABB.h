#pragma once

#include "RE/B/BSMultiBoundShape.h"

namespace RE
{
	class __declspec(novtable) BSMultiBoundAABB :
		public BSMultiBoundShape  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::BSMultiBoundAABB };
		static constexpr auto VTABLE{ VTABLE::BSMultiBoundAABB };
		static constexpr auto Ni_RTTI{ Ni_RTTI::BSMultiBoundAABB };

		// add
		virtual bool IntersectPlane(const NiPlane* a_plane);  // 32

		// members
		NiPoint3A center;       // 20
		NiPoint3A halfExtents;  // 30
	};
	static_assert(sizeof(BSMultiBoundAABB) == 0x40);
}
