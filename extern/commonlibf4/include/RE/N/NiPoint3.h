#pragma once

#include "RE/N/NiPoint2.h"

namespace RE
{
	class NiPoint3
	{
	public:
		static const NiPoint3 ZERO;
		static const NiPoint3 UNIT;
		static const NiPoint3 UNIT_X;
		static const NiPoint3 UNIT_Y;
		static const NiPoint3 UNIT_Z;

		constexpr NiPoint3() noexcept = default;
		constexpr NiPoint3(const NiPoint2& a_point) noexcept;
		constexpr NiPoint3(float a_x, float a_y, float a_z) noexcept;

		constexpr float&       operator[](std::size_t a_pos) noexcept;
		constexpr const float& operator[](std::size_t a_pos) const noexcept;
		constexpr bool         operator==(const NiPoint3& a_rhs) const noexcept;
		constexpr bool         operator!=(const NiPoint3& a_rhs) const noexcept;
		constexpr bool         operator<(const NiPoint3& a_rhs) const noexcept;
		constexpr bool         operator>(const NiPoint3& a_rhs) const noexcept;
		constexpr NiPoint3     operator+(const NiPoint3& a_rhs) const noexcept;
		constexpr NiPoint3     operator-(const NiPoint3& a_rhs) const noexcept;
		constexpr NiPoint3     operator*(const NiPoint3& a_rhs) const noexcept;
		constexpr NiPoint3     operator/(const NiPoint3& a_rhs) const noexcept;
		constexpr NiPoint3     operator*(float a_scalar) const noexcept;
		constexpr NiPoint3     operator/(float a_scalar) const noexcept;
		constexpr NiPoint3     operator-() const noexcept;
		constexpr NiPoint3&    operator+=(const NiPoint3& a_rhs) noexcept;
		constexpr NiPoint3&    operator-=(const NiPoint3& a_rhs) noexcept;
		constexpr NiPoint3&    operator*=(const NiPoint3& a_rhs) noexcept;
		constexpr NiPoint3&    operator/=(const NiPoint3& a_rhs) noexcept;
		constexpr NiPoint3&    operator+=(float a_scalar) noexcept;
		constexpr NiPoint3&    operator-=(float a_scalar) noexcept;
		constexpr NiPoint3&    operator*=(float a_scalar) noexcept;
		constexpr NiPoint3&    operator/=(float a_scalar) noexcept;

		constexpr NiPoint3 Cross(const NiPoint3& a_point) const noexcept;
		constexpr float    Dot(const NiPoint3& a_point) const noexcept;
		constexpr float    GetSquaredDistance(const NiPoint3& a_point) const noexcept;
		constexpr float    SqrLength() const noexcept;

		float    GetDistance(const NiPoint3& a_point) const noexcept;
		float    GetZAngleFromVector() const;
		float    Length() const noexcept;
		NiPoint3 UnitCross(const NiPoint3& a_point) const noexcept;
		float    Unitize() noexcept;

		// members
		float x{ 0.0F };  // 0x00
		float y{ 0.0F };  // 0x04
		float z{ 0.0F };  // 0x08
	};
	static_assert(sizeof(NiPoint3) == 0xC);

	class alignas(0x10) NiPoint3A :
		public NiPoint3
	{
	public:
		static const NiPoint3A ZERO;
		static const NiPoint3A UNIT;
		static const NiPoint3A UNIT_X;
		static const NiPoint3A UNIT_Y;
		static const NiPoint3A UNIT_Z;

		using NiPoint3::NiPoint3;
	};
	static_assert(sizeof(NiPoint3A) == 0x10);
}

namespace RE
{
	constexpr NiPoint3::NiPoint3(const NiPoint2& a_point) noexcept :
		x(a_point.x), y(a_point.y)
	{}

	constexpr NiPoint3::NiPoint3(float a_x, float a_y, float a_z) noexcept :
		x(a_x), y(a_y), z(a_z)
	{}

	constexpr float& NiPoint3::operator[](std::size_t a_pos) noexcept
	{
		assert(a_pos < 3);
		return std::addressof(x)[a_pos];
	}

	constexpr const float& NiPoint3::operator[](std::size_t a_pos) const noexcept
	{
		assert(a_pos < 3);
		return std::addressof(x)[a_pos];
	}

	constexpr bool NiPoint3::operator==(const NiPoint3& a_rhs) const noexcept
	{
		return (x == a_rhs.x) && (y == a_rhs.y) && (z == a_rhs.z);
	}

	constexpr bool NiPoint3::operator!=(const NiPoint3& a_rhs) const noexcept
	{
		return !operator==(a_rhs);
	}

	constexpr bool NiPoint3::operator<(const NiPoint3& a_rhs) const noexcept
	{
		return std::tie(x, y, z) < std::tie(a_rhs.x, a_rhs.y, a_rhs.z);
	}

	constexpr bool NiPoint3::operator>(const NiPoint3& a_rhs) const noexcept
	{
		return std::tie(x, y, z) > std::tie(a_rhs.x, a_rhs.y, a_rhs.z);
	}

	constexpr NiPoint3 NiPoint3::operator+(const NiPoint3& a_rhs) const noexcept
	{
		return NiPoint3(x + a_rhs.x, y + a_rhs.y, z + a_rhs.z);
	}

	constexpr NiPoint3 NiPoint3::operator-(const NiPoint3& a_rhs) const noexcept
	{
		return NiPoint3(x - a_rhs.x, y - a_rhs.y, z - a_rhs.z);
	}

	constexpr NiPoint3 NiPoint3::operator*(const NiPoint3& a_rhs) const noexcept
	{
		return NiPoint3(x * a_rhs.x, y * a_rhs.y, z * a_rhs.z);
	}

	constexpr NiPoint3 NiPoint3::operator/(const NiPoint3& a_rhs) const noexcept
	{
		return NiPoint3(x / a_rhs.x, y / a_rhs.y, z / a_rhs.z);
	}

	constexpr NiPoint3 NiPoint3::operator*(float a_scalar) const noexcept
	{
		return NiPoint3(x * a_scalar, y * a_scalar, z * a_scalar);
	}

	constexpr NiPoint3 NiPoint3::operator/(float a_scalar) const noexcept
	{
		return NiPoint3(x / a_scalar, y / a_scalar, z / a_scalar);
	}

	constexpr NiPoint3 NiPoint3::operator-() const noexcept
	{
		return NiPoint3(-x, -y, -z);
	}

	constexpr NiPoint3& NiPoint3::operator+=(const NiPoint3& a_rhs) noexcept
	{
		x += a_rhs.x;
		y += a_rhs.y;
		z += a_rhs.z;
		return *this;
	}

	constexpr NiPoint3& NiPoint3::operator-=(const NiPoint3& a_rhs) noexcept
	{
		x -= a_rhs.x;
		y -= a_rhs.y;
		z -= a_rhs.z;
		return *this;
	}

	constexpr NiPoint3& NiPoint3::operator*=(const NiPoint3& a_rhs) noexcept
	{
		x *= a_rhs.x;
		y *= a_rhs.y;
		z *= a_rhs.z;
		return *this;
	}

	constexpr NiPoint3& NiPoint3::operator/=(const NiPoint3& a_rhs) noexcept
	{
		x /= a_rhs.x;
		y /= a_rhs.y;
		z /= a_rhs.z;
		return *this;
	}

	constexpr NiPoint3& NiPoint3::operator+=(float a_scalar) noexcept
	{
		x += a_scalar;
		y += a_scalar;
		z += a_scalar;
		return *this;
	}

	constexpr NiPoint3& NiPoint3::operator-=(float a_scalar) noexcept
	{
		x -= a_scalar;
		y -= a_scalar;
		z -= a_scalar;
		return *this;
	}

	constexpr NiPoint3& NiPoint3::operator*=(float a_scalar) noexcept
	{
		x *= a_scalar;
		y *= a_scalar;
		z *= a_scalar;
		return *this;
	}

	constexpr NiPoint3& NiPoint3::operator/=(float a_scalar) noexcept
	{
		x /= a_scalar;
		y /= a_scalar;
		z /= a_scalar;
		return *this;
	}

	constexpr NiPoint3 NiPoint3::Cross(const NiPoint3& a_point) const noexcept
	{
		return NiPoint3(
			y * a_point.z - z * a_point.y,
			z * a_point.x - x * a_point.z,
			x * a_point.y - y * a_point.x);
	}

	constexpr float NiPoint3::Dot(const NiPoint3& a_point) const noexcept
	{
		return x * a_point.x + y * a_point.y + z * a_point.z;
	}

	constexpr float NiPoint3::GetSquaredDistance(const NiPoint3& a_point) const noexcept
	{
		const float dx = a_point.x - x;
		const float dy = a_point.y - y;
		const float dz = a_point.z - z;
		return dx * dx + dy * dy + dz * dz;
	}

	constexpr float NiPoint3::SqrLength() const noexcept
	{
		return x * x + y * y + z * z;
	}

	inline const NiPoint3  NiPoint3::ZERO = { 0.0F, 0.0F, 0.0F };
	inline const NiPoint3  NiPoint3::UNIT = { 1.0F, 1.0F, 1.0F };
	inline const NiPoint3  NiPoint3::UNIT_X = { 1.0F, 0.0F, 0.0F };
	inline const NiPoint3  NiPoint3::UNIT_Y = { 0.0F, 1.0F, 0.0F };
	inline const NiPoint3  NiPoint3::UNIT_Z = { 0.0F, 0.0F, 1.0F };
	inline const NiPoint3A NiPoint3A::ZERO = { 0.0F, 0.0F, 0.0F };
	inline const NiPoint3A NiPoint3A::UNIT = { 1.0F, 1.0F, 1.0F };
	inline const NiPoint3A NiPoint3A::UNIT_X = { 1.0F, 0.0F, 0.0F };
	inline const NiPoint3A NiPoint3A::UNIT_Y = { 0.0F, 1.0F, 0.0F };
	inline const NiPoint3A NiPoint3A::UNIT_Z = { 0.0F, 0.0F, 1.0F };
}

template <>
struct std::formatter<RE::NiPoint3>
{
	template <class ParseContext>
	constexpr auto parse(ParseContext& a_ctx)
	{
		return a_ctx.begin();
	}

	template <class FormatContext>
	constexpr auto format(const RE::NiPoint3& a_point, FormatContext& a_ctx) const
	{
		return format_to(a_ctx.out(), "({}, {}, {})", a_point.x, a_point.y, a_point.z);
	}
};
