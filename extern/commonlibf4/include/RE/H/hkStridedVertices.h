#pragma once

#include "RE/H/hkArray.h"
#include "RE/H/hkVector4.h"

namespace RE
{
	class hkStridedVertices
	{
	public:
		constexpr hkStridedVertices() = default;

		template <class T>
		constexpr hkStridedVertices(const T* a_vertices, const std::int32_t a_numVertices)
		{
			static_assert(sizeof(T) > sizeof(float));
			m_vertices = (const float*)a_vertices;
			m_numVertices = a_numVertices;
			m_striding = sizeof(T);
		}

		hkStridedVertices(const hkArrayBase<hkVector4>& a_vertices)
		{
			m_vertices = (const float*)a_vertices.begin();
			m_numVertices = a_vertices.size();
			m_striding = sizeof(hkVector4);
		}

		constexpr std::int32_t size() const
		{
			return m_numVertices;
		}

		// members
		const float* m_vertices{ nullptr };  // 0x00
		std::int32_t m_numVertices{ 0 };     // 0x08
		std::int32_t m_striding{ 0 };        // 0x0C - sizeof(T)
	};
	static_assert(sizeof(hkStridedVertices) == 0x10);
}
