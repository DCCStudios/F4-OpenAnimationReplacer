#pragma once

#include "RE/H/hkVector4.h"
#include "RE/H/hknpConvexPolytopeShape.h"

namespace RE
{
	class __declspec(novtable) hknpCapsuleShape :
		public hknpConvexPolytopeShape
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hknpCapsuleShape };
		inline static constexpr auto VTABLE{ VTABLE::hknpCapsuleShape };

		void Init(const hkVector4f& a_vertexA, const hkVector4f& a_vertexB)
		{
			using func_t = decltype(&hknpCapsuleShape::Init);
			static REL::Relocation<func_t> func{ ID::hknpCapsuleShape::Init };
			func(this, a_vertexA, a_vertexB);
		}

		static hknpCapsuleShape* CreateCapsuleShape(const hkVector4f& a_vertexA, const hkVector4f& a_vertexB, float a_radius)
		{
			using func_t = decltype(&hknpCapsuleShape::CreateCapsuleShape);
			static REL::Relocation<func_t> func{ ID::hknpCapsuleShape::CreateCapsuleShape };
			return func(a_vertexA, a_vertexB, a_radius);
		}

		// members
		hkVector4f m_vertexA;  // 0x50
		hkVector4f m_vertexB;  // 0x60
	};
	static_assert(sizeof(hknpCapsuleShape) == 0x70);
}
