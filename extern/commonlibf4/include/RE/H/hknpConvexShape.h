#pragma once

#include "RE/H/hkRelArray.h"
#include "RE/H/hknpShape.h"

namespace RE
{
	class hkAabb;
	class hkStridedVertices;
	class hkTransformf;
	class hkVector4f;
	class hkcdVertex;

	class __declspec(novtable) hknpConvexShape :
		public hknpShape
	{
	public:
		static constexpr auto RTTI{ RTTI::hknpConvexShape };
		static constexpr auto VTABLE{ VTABLE::hknpConvexShape };

		struct BuildConfig
		{
			float         simplificationTolerance{ 0.0099999998f };  // 0x00
			hkBool        shrinkByRadius{ true };                    // 0x04
			float         featurePreservationFactor{ 0.0f };         // 0x08
			hkBool        buildFaces{ true };                        // 0x0C
			hkBool        buildMassProperties{ false };              // 0x0D
			MassConfig    massConfig;                                // 0x10
			hkTransformf* extraTransform{ nullptr };                 // 0x20
			std::uint32_t sizeOfBaseClass{ 80 };                     // 0x28
			std::uint32_t maxNumVertices{ 252 };                     // 0x2C
			hkBool        checkForDegenerateFaces{ false };          // 0x30
			hkBool        verbose{ false };                          // 0x31
		};
		static_assert(sizeof(BuildConfig) == 0x38);

		static hknpConvexShape* CreateFromAabb(const hkAabb& a_aabb, const float a_radius, const BuildConfig& a_config)
		{
			using func_t = decltype(&hknpConvexShape::CreateFromAabb);
			static REL::Relocation<func_t> func{ ID::hknpConvexShape::CreateFromAabb };
			return func(a_aabb, a_radius, a_config);
		}

		static hknpConvexShape* CreateFromCylinder(const hkVector4f& a_halfExtent, const std::int32_t a_numVertices, const float a_radius, const BuildConfig& a_config)
		{
			using func_t = decltype(&hknpConvexShape::CreateFromCylinder);
			static REL::Relocation<func_t> func{ ID::hknpConvexShape::CreateFromCylinder };
			return func(a_halfExtent, a_numVertices, a_radius, a_config);
		}

		static hknpConvexShape* CreateFromHalfExtents(const hkVector4f& a_halfExtent, const float a_radius, const BuildConfig& a_config)
		{
			using func_t = decltype(&hknpConvexShape::CreateFromHalfExtents);
			static REL::Relocation<func_t> func{ ID::hknpConvexShape::CreateFromHalfExtents };
			return func(a_halfExtent, a_radius, a_config);
		}

		static hknpConvexShape* CreateFromIndexedVertices(const hkVector4f* a_vertexBuffer, const std::uint16_t* a_indexBuffer, std::int32_t a_numVertices, const float a_radius, const BuildConfig& a_config)
		{
			using func_t = decltype(&hknpConvexShape::CreateFromIndexedVertices);
			static REL::Relocation<func_t> func{ ID::hknpConvexShape::CreateFromIndexedVertices };
			return func(a_vertexBuffer, a_indexBuffer, a_numVertices, a_radius, a_config);
		}

		static hknpConvexShape* CreateFromVertices(const hkStridedVertices& a_vertices, const float a_radius, const BuildConfig& a_config)
		{
			using func_t = decltype(&hknpConvexShape::CreateFromVertices);
			static REL::Relocation<func_t> func{ ID::hknpConvexShape::CreateFromVertices };
			return func(a_vertices, a_radius, a_config);
		}

		// members
		hkRelArray<hkcdVertex> vertices;     // 0x30
		std::byte              pad34[0x0C];  // 0x34
	};
	static_assert(sizeof(hknpConvexShape) == 0x40);
}
