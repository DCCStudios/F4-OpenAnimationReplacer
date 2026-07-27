#pragma once

#include "RE/H/hkVector4.h"

namespace RE
{
	class hkMatrix3f
	{
	public:
		constexpr hkMatrix3f() noexcept = default;
		constexpr hkMatrix3f(const hkVector4f& a_vec0, const hkVector4f& a_vec1, const hkVector4f& a_vec2) noexcept;
		constexpr hkMatrix3f(
			float a_x0, float a_y0, float a_z0, float a_w0,
			float a_x1, float a_y1, float a_z1, float a_w1,
			float a_x2, float a_y2, float a_z2, float a_w2) noexcept;

		constexpr hkVector4f&       operator[](std::size_t a_pos) noexcept;
		constexpr const hkVector4f& operator[](std::size_t a_pos) const noexcept;
		constexpr bool              operator==(const hkMatrix3f& a_rhs) const noexcept;
		constexpr bool              operator!=(const hkMatrix3f& a_rhs) const noexcept;
		constexpr hkMatrix3f        operator*(const hkMatrix3f& a_rhs) const noexcept;
		constexpr hkMatrix3f        operator*(float a_scalar) const noexcept;

		// members
		hkVector4f m_col[3];  // 0x00
	};
	static_assert(sizeof(hkMatrix3f) == 0x30);

	using hkMatrix3 = hkMatrix3f;
}

namespace RE
{
	constexpr hkMatrix3f::hkMatrix3f(const hkVector4f& a_vec0, const hkVector4f& a_vec1, const hkVector4f& a_vec2) noexcept
	{
		m_col[0] = a_vec0;
		m_col[1] = a_vec1;
		m_col[2] = a_vec2;
	}

	constexpr hkMatrix3f::hkMatrix3f(
		float a_x0, float a_y0, float a_z0, float a_w0,
		float a_x1, float a_y1, float a_z1, float a_w1,
		float a_x2, float a_y2, float a_z2, float a_w2) noexcept
	{
		m_col[0] = { a_x0, a_y0, a_z0, a_w0 };
		m_col[1] = { a_x1, a_y1, a_z1, a_w1 };
		m_col[2] = { a_x2, a_y2, a_z2, a_w2 };
	}

	constexpr hkVector4f& hkMatrix3f::operator[](std::size_t a_pos) noexcept
	{
		assert(a_pos < 3);
		return m_col[a_pos];
	}

	constexpr const hkVector4f& hkMatrix3f::operator[](std::size_t a_pos) const noexcept
	{
		assert(a_pos < 3);
		return m_col[a_pos];
	}

	constexpr bool hkMatrix3f::operator==(const hkMatrix3f& a_rhs) const noexcept
	{
		return (m_col[0] == a_rhs[0]) && (m_col[1] == a_rhs[1]) && (m_col[2] == a_rhs[2]);
	}

	constexpr bool hkMatrix3f::operator!=(const hkMatrix3f& a_rhs) const noexcept
	{
		return !operator==(a_rhs);
	}

	constexpr hkMatrix3f hkMatrix3f::operator*(const hkMatrix3f& a_rhs) const noexcept
	{
		hkMatrix3f result;
		result[0][0] = m_col[0][0] * a_rhs[0][0] + m_col[0][1] * a_rhs[1][0] + m_col[0][2] * a_rhs[2][0];
		result[1][0] = m_col[1][0] * a_rhs[0][0] + m_col[1][1] * a_rhs[1][0] + m_col[1][2] * a_rhs[2][0];
		result[2][0] = m_col[2][0] * a_rhs[0][0] + m_col[2][1] * a_rhs[1][0] + m_col[2][2] * a_rhs[2][0];
		result[0][1] = m_col[0][0] * a_rhs[0][1] + m_col[0][1] * a_rhs[1][1] + m_col[0][2] * a_rhs[2][1];
		result[1][1] = m_col[1][0] * a_rhs[0][1] + m_col[1][1] * a_rhs[1][1] + m_col[1][2] * a_rhs[2][1];
		result[2][1] = m_col[2][0] * a_rhs[0][1] + m_col[2][1] * a_rhs[1][1] + m_col[2][2] * a_rhs[2][1];
		result[0][2] = m_col[0][0] * a_rhs[0][2] + m_col[0][1] * a_rhs[1][2] + m_col[0][2] * a_rhs[2][2];
		result[1][2] = m_col[1][0] * a_rhs[0][2] + m_col[1][1] * a_rhs[1][2] + m_col[1][2] * a_rhs[2][2];
		result[2][2] = m_col[2][0] * a_rhs[0][2] + m_col[2][1] * a_rhs[1][2] + m_col[2][2] * a_rhs[2][2];
		return result;
	}

	constexpr hkMatrix3f hkMatrix3f::operator*(float a_scalar) const noexcept
	{
		hkMatrix3f result;
		result[0][0] = m_col[0][0] * a_scalar;
		result[0][1] = m_col[0][1] * a_scalar;
		result[0][2] = m_col[0][2] * a_scalar;
		result[1][0] = m_col[1][0] * a_scalar;
		result[1][1] = m_col[1][1] * a_scalar;
		result[1][2] = m_col[1][2] * a_scalar;
		result[2][0] = m_col[2][0] * a_scalar;
		result[2][1] = m_col[2][1] * a_scalar;
		result[2][2] = m_col[2][2] * a_scalar;
		return result;
	}
}
