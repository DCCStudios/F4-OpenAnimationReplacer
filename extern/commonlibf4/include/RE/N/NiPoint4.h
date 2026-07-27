#pragma once

#include "RE/N/NiPoint3.h"

namespace RE
{
	class NiPoint4
	{
	public:
		static const NiPoint4 ZERO;
		static const NiPoint4 IDENTITY0;
		static const NiPoint4 IDENTITY1;
		static const NiPoint4 IDENTITY2;
		static const NiPoint4 IDENTITY3;

		constexpr NiPoint4() noexcept = default;
		constexpr NiPoint4(const NiPoint2& a_point) noexcept;
		constexpr NiPoint4(const NiPoint3& a_point) noexcept;
		constexpr NiPoint4(float a_x, float a_y, float a_z, float a_w) noexcept;

		constexpr float&       operator[](std::size_t a_pos) noexcept;
		constexpr const float& operator[](std::size_t a_pos) const noexcept;
		constexpr bool         operator==(const NiPoint4& a_rhs) const noexcept;
		constexpr bool         operator!=(const NiPoint4& a_rhs) const noexcept;
		constexpr bool         operator<(const NiPoint4& a_rhs) const noexcept;
		constexpr bool         operator>(const NiPoint4& a_rhs) const noexcept;
		constexpr NiPoint4     operator+(const NiPoint4& a_rhs) const noexcept;
		constexpr NiPoint4     operator-(const NiPoint4& a_rhs) const noexcept;
		constexpr NiPoint4     operator*(const NiPoint4& a_rhs) const noexcept;
		constexpr NiPoint4     operator/(const NiPoint4& a_rhs) const noexcept;
		constexpr NiPoint4     operator*(float a_scalar) const noexcept;
		constexpr NiPoint4     operator/(float a_scalar) const noexcept;
		constexpr NiPoint4     operator-() const noexcept;
		constexpr NiPoint4&    operator+=(const NiPoint4& a_rhs) noexcept;
		constexpr NiPoint4&    operator-=(const NiPoint4& a_rhs) noexcept;
		constexpr NiPoint4&    operator*=(const NiPoint4& a_rhs) noexcept;
		constexpr NiPoint4&    operator/=(const NiPoint4& a_rhs) noexcept;
		constexpr NiPoint4&    operator+=(float a_scalar) noexcept;
		constexpr NiPoint4&    operator-=(float a_scalar) noexcept;
		constexpr NiPoint4&    operator*=(float a_scalar) noexcept;
		constexpr NiPoint4&    operator/=(float a_scalar) noexcept;

		// members
		float x{ 0.0F };  // 0x00
		float y{ 0.0F };  // 0x04
		float z{ 0.0F };  // 0x08
		float w{ 0.0F };  // 0x0C
	};
	static_assert(sizeof(NiPoint4) == 0x10);
}

namespace RE
{
	constexpr NiPoint4::NiPoint4(const NiPoint2& a_point) noexcept :
		x(a_point.x), y(a_point.y)
	{}

	constexpr NiPoint4::NiPoint4(const NiPoint3& a_point) noexcept :
		x(a_point.x), y(a_point.y), z(a_point.z)
	{}

	constexpr NiPoint4::NiPoint4(float a_x, float a_y, float a_z, float a_w) noexcept :
		x(a_x), y(a_y), z(a_z), w(a_w)
	{}

	constexpr float& NiPoint4::operator[](std::size_t a_pos) noexcept
	{
		assert(a_pos < 4);
		return std::addressof(x)[a_pos];
	}

	constexpr const float& NiPoint4::operator[](std::size_t a_pos) const noexcept
	{
		assert(a_pos < 4);
		return std::addressof(x)[a_pos];
	}

	constexpr bool NiPoint4::operator==(const NiPoint4& a_rhs) const noexcept
	{
		return (x == a_rhs.x) && (y == a_rhs.y) && (z == a_rhs.z) && (w == a_rhs.w);
	}

	constexpr bool NiPoint4::operator!=(const NiPoint4& a_rhs) const noexcept
	{
		return !operator==(a_rhs);
	}

	constexpr bool NiPoint4::operator<(const NiPoint4& a_rhs) const noexcept
	{
		return std::tie(x, y, z, w) < std::tie(a_rhs.x, a_rhs.y, a_rhs.z, a_rhs.w);
	}

	constexpr bool NiPoint4::operator>(const NiPoint4& a_rhs) const noexcept
	{
		return std::tie(x, y, z, w) > std::tie(a_rhs.x, a_rhs.y, a_rhs.z, a_rhs.w);
	}

	constexpr NiPoint4 NiPoint4::operator+(const NiPoint4& a_rhs) const noexcept
	{
		return NiPoint4(x + a_rhs.x, y + a_rhs.y, z + a_rhs.z, w + a_rhs.w);
	}

	constexpr NiPoint4 NiPoint4::operator-(const NiPoint4& a_rhs) const noexcept
	{
		return NiPoint4(x - a_rhs.x, y - a_rhs.y, z - a_rhs.z, w - a_rhs.w);
	}

	constexpr NiPoint4 NiPoint4::operator*(const NiPoint4& a_rhs) const noexcept
	{
		return NiPoint4(x * a_rhs.x, y * a_rhs.y, z * a_rhs.z, w * a_rhs.w);
	}

	constexpr NiPoint4 NiPoint4::operator/(const NiPoint4& a_rhs) const noexcept
	{
		return NiPoint4(x / a_rhs.x, y / a_rhs.y, z / a_rhs.z, w / a_rhs.w);
	}

	constexpr NiPoint4 NiPoint4::operator*(float a_scalar) const noexcept
	{
		return NiPoint4(x * a_scalar, y * a_scalar, z * a_scalar, w * a_scalar);
	}

	constexpr NiPoint4 NiPoint4::operator/(float a_scalar) const noexcept
	{
		return operator*(1.0F / a_scalar);
	}

	constexpr NiPoint4 NiPoint4::operator-() const noexcept
	{
		return NiPoint4(-x, -y, -z, -w);
	}

	constexpr NiPoint4& NiPoint4::operator+=(const NiPoint4& a_rhs) noexcept
	{
		x += a_rhs.x;
		y += a_rhs.y;
		z += a_rhs.z;
		w += a_rhs.w;
		return *this;
	}

	constexpr NiPoint4& NiPoint4::operator-=(const NiPoint4& a_rhs) noexcept
	{
		x -= a_rhs.x;
		y -= a_rhs.y;
		z -= a_rhs.z;
		w -= a_rhs.w;
		return *this;
	}

	constexpr NiPoint4& NiPoint4::operator*=(const NiPoint4& a_rhs) noexcept
	{
		x *= a_rhs.x;
		y *= a_rhs.y;
		z *= a_rhs.z;
		w *= a_rhs.w;
		return *this;
	}

	constexpr NiPoint4& NiPoint4::operator/=(const NiPoint4& a_rhs) noexcept
	{
		x /= a_rhs.x;
		y /= a_rhs.y;
		z /= a_rhs.z;
		w /= a_rhs.w;
		return *this;
	}

	constexpr NiPoint4& NiPoint4::operator+=(float a_scalar) noexcept
	{
		x += a_scalar;
		y += a_scalar;
		z += a_scalar;
		w += a_scalar;
		return *this;
	}

	constexpr NiPoint4& NiPoint4::operator-=(float a_scalar) noexcept
	{
		x -= a_scalar;
		y -= a_scalar;
		z -= a_scalar;
		w -= a_scalar;
		return *this;
	}

	constexpr NiPoint4& NiPoint4::operator*=(float a_scalar) noexcept
	{
		x *= a_scalar;
		y *= a_scalar;
		z *= a_scalar;
		w *= a_scalar;
		return *this;
	}

	constexpr NiPoint4& NiPoint4::operator/=(float a_scalar) noexcept
	{
		x /= a_scalar;
		y /= a_scalar;
		z /= a_scalar;
		w /= a_scalar;
		return *this;
	}

	inline const NiPoint4 NiPoint4::ZERO = { 0.0F, 0.0F, 0.0F, 0.0F };
	inline const NiPoint4 NiPoint4::IDENTITY0 = { 1.0F, 0.0F, 0.0F, 0.0F };
	inline const NiPoint4 NiPoint4::IDENTITY1 = { 0.0F, 1.0F, 0.0F, 0.0F };
	inline const NiPoint4 NiPoint4::IDENTITY2 = { 0.0F, 0.0F, 1.0F, 0.0F };
	inline const NiPoint4 NiPoint4::IDENTITY3 = { 0.0F, 0.0F, 0.0F, 1.0F };
}

template <>
struct std::formatter<RE::NiPoint4>
{
	template <class ParseContext>
	constexpr auto parse(ParseContext& a_ctx)
	{
		return a_ctx.begin();
	}

	template <class FormatContext>
	constexpr auto format(const RE::NiPoint4& a_point, FormatContext& a_ctx) const
	{
		return format_to(a_ctx.out(), "({}, {}, {}, {})", a_point.x, a_point.y, a_point.z, a_point.w);
	}
};
