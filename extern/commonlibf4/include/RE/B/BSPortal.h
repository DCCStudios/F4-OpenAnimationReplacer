#pragma once

#include "RE/B/BSMultiBoundRoom.h"
#include "RE/B/BSOcclusionPlane.h"
#include "RE/B/BSPortalSharedNode.h"

namespace RE
{
	class __declspec(novtable) alignas(16) BSPortal :
		public BSOcclusionPlane  // 00
	{
		inline static constexpr auto RTTI{ RTTI::BSPortal };
		inline static constexpr auto VTABLE{ VTABLE::BSPortal };
		inline static constexpr auto Ni_RTTI{ Ni_RTTI::BSPortal };

		// members
		BSMultiBoundRoom*             multiboundRoom[2];  // 130
		NiPointer<BSPortalSharedNode> sharedGeometry;     // 140
	};
	static_assert(sizeof(BSPortal) == 0x150);
}
