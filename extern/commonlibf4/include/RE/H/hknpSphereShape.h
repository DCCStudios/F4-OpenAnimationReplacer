#pragma once

#include "RE/H/hknpConvexShape.h"

namespace RE
{
	class __declspec(novtable) hknpSphereShape :
		public hknpConvexShape
	{
	public:
		static constexpr auto RTTI{ RTTI::hknpSphereShape };
		static constexpr auto VTABLE{ VTABLE::hknpSphereShape };

		static hknpSphereShape* CreateSphereShape(const hkVector4f& a_center, float a_radius)
		{
			using func_t = decltype(&hknpSphereShape::CreateSphereShape);
			static REL::Relocation<func_t> func{ ID::hknpSphereShape::CreateSphereShape };
			return func(a_center, a_radius);
		}
	};
	static_assert(sizeof(hknpSphereShape) == 0x40);
}
