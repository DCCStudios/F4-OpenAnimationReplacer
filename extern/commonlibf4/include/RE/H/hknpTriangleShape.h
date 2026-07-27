#pragma once

#include "RE/H/hknpConvexPolytopeShape.h"

namespace RE
{
	class __declspec(novtable) hknpTriangleShape :
		public hknpConvexPolytopeShape
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hknpTriangleShape };
		inline static constexpr auto VTABLE{ VTABLE::hknpTriangleShape };

		static hknpTriangleShape* CreateEmptyTriangleShape(const float a_radius)
		{
			using func_t = decltype(&hknpTriangleShape::CreateEmptyTriangleShape);
			static REL::Relocation<func_t> func{ ID::hknpTriangleShape::CreateEmptyTriangleShape };
			return func(a_radius);
		}

		static hknpTriangleShape* CreateTriangleShape(const hkVector4f& a_vertexA, const hkVector4f& a_vertexB, const hkVector4f& a_vertexC, const float a_radius)
		{
			auto shape = CreateEmptyTriangleShape(a_radius);
			shape->SetVertices(a_vertexA, a_vertexB, a_vertexC);
			return shape;
		}

		void SetVertices(const hkVector4f& a_vertexA, const hkVector4f& a_vertexB, const hkVector4f& a_vertexC)
		{
			using func_t = decltype(&hknpTriangleShape::SetVertices);
			static REL::Relocation<func_t> func{ ID::hknpTriangleShape::SetVertices };
			func(this, a_vertexA, a_vertexB, a_vertexC);
		}
	};
}
