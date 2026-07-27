#pragma once

#include "RE/H/hknpConvexShape.h"

namespace RE
{
	class hkVector4f;

	class __declspec(novtable) hknpConvexPolytopeShape :
		public hknpConvexShape
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hknpConvexPolytopeShape };
		inline static constexpr auto VTABLE{ VTABLE::hknpConvexPolytopeShape };

		struct Face
		{
			std::uint16_t firstIndex;
			std::uint8_t  numIndices;
			std::uint8_t  minHalfAngle;
		};

		static hknpConvexPolytopeShape* CreateFromVerticesInternal(const hkStridedVertices& a_vertices, const float a_radius, const BuildConfig& a_config)
		{
			using func_t = decltype(&hknpConvexPolytopeShape::CreateFromVerticesInternal);
			static REL::Relocation<func_t> func{ ID::hknpConvexPolytopeShape::CreateFromVerticesInternal };
			return func(a_vertices, a_radius, a_config);
		}

		// members
		hkRelArray<hkVector4f>   planes;   // 0x40
		hkRelArray<Face>         faces;    // 0x44
		hkRelArray<std::uint8_t> indices;  // 0x48
	};
	static_assert(sizeof(hknpConvexPolytopeShape) == 0x50);
}
