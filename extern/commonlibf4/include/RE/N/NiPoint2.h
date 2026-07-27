#pragma once

namespace RE
{
	class NiPoint2
	{
	public:
		static const NiPoint2 ZERO;
		static const NiPoint2 UNIT;
		static const NiPoint2 UNIT_X;
		static const NiPoint2 UNIT_Y;

		constexpr NiPoint2() noexcept = default;
		constexpr NiPoint2(float a_x, float a_y) noexcept;

		constexpr float&       operator[](std::size_t a_pos) noexcept;
		constexpr const float& operator[](std::size_t a_pos) const noexcept;
		constexpr bool         operator==(const NiPoint2& a_rhs) const noexcept;
		constexpr bool         operator!=(const NiPoint2& a_rhs) const noexcept;
		constexpr bool         operator<(const NiPoint2& a_rhs) const noexcept;
		constexpr bool         operator>(const NiPoint2& a_rhs) const noexcept;
		constexpr NiPoint2     operator+(const NiPoint2& a_rhs) const noexcept;
		constexpr NiPoint2     operator-(const NiPoint2& a_rhs) const noexcept;
		constexpr NiPoint2     operator*(const NiPoint2& a_rhs) const noexcept;
		constexpr NiPoint2     operator/(const NiPoint2& a_rhs) const noexcept;
		constexpr NiPoint2     operator*(float a_scalar) const noexcept;
		constexpr NiPoint2     operator/(float a_scalar) const noexcept;
		constexpr NiPoint2     operator-() const noexcept;
		constexpr NiPoint2&    operator+=(const NiPoint2& a_rhs) noexcept;
		constexpr NiPoint2&    operator-=(const NiPoint2& a_rhs) noexcept;
		constexpr NiPoint2&    operator*=(const NiPoint2& a_rhs) noexcept;
		constexpr NiPoint2&    operator/=(const NiPoint2& a_rhs) noexcept;
		constexpr NiPoint2&    operator+=(float a_scalar) noexcept;
		constexpr NiPoint2&    operator-=(float a_scalar) noexcept;
		constexpr NiPoint2&    operator*=(float a_scalar) noexcept;
		constexpr NiPoint2&    operator/=(float a_scalar) noexcept;

		// members
		float x{ 0.0F };  // 0x00
		float y{ 0.0F };  // 0x04
	};
	static_assert(sizeof(NiPoint2) == 0x8);
}

namespace RE
{
	constexpr NiPoint2::NiPoint2(float a_x, float a_y) noexcept :
		x(a_x), y(a_y)
	{}

	constexpr float& NiPoint2::operator[](std::size_t a_pos) noexcept
	{
		assert(a_pos < 2);
		return std::addressof(x)[a_pos];
	}

	constexpr const float& NiPoint2::operator[](std::size_t a_pos) const noexcept
	{
		assert(a_pos < 2);
		return std::addressof(x)[a_pos];
	}

	constexpr bool NiPoint2::operator==(const NiPoint2& a_rhs) const noexcept
	{
		return (x == a_rhs.x) && (y == a_rhs.y);
	}

	constexpr bool NiPoint2::operator!=(const NiPoint2& a_rhs) const noexcept
	{
		return !operator==(a_rhs);
	}

	constexpr bool NiPoint2::operator<(const NiPoint2& a_rhs) const noexcept
	{
		return std::tie(x, y) < std::tie(a_rhs.x, a_rhs.y);
	}

	constexpr bool NiPoint2::operator>(const NiPoint2& a_rhs) const noexcept
	{
		return std::tie(x, y) > std::tie(a_rhs.x, a_rhs.y);
	}

	constexpr NiPoint2 NiPoint2::operator+(const NiPoint2& a_rhs) const noexcept
	{
		return NiPoint2(x + a_rhs.x, y + a_rhs.y);
	}

	constexpr NiPoint2 NiPoint2::operator-(const NiPoint2& a_rhs) const noexcept
	{
		return NiPoint2(x - a_rhs.x, y - a_rhs.y);
	}

	constexpr NiPoint2 NiPoint2::operator*(const NiPoint2& a_rhs) const noexcept
	{
		return NiPoint2(x * a_rhs.x, y * a_rhs.y);
	}

	constexpr NiPoint2 NiPoint2::operator/(const NiPoint2& a_rhs) const noexcept
	{
		return NiPoint2(x / a_rhs.x, y / a_rhs.y);
	}

	constexpr NiPoint2 NiPoint2::operator*(float a_scalar) const noexcept
	{
		return NiPoint2(x * a_scalar, y * a_scalar);
	}

	constexpr NiPoint2 NiPoint2::operator/(float a_scalar) const noexcept
	{
		return NiPoint2(x / a_scalar, y / a_scalar);
	}

	constexpr NiPoint2 NiPoint2::operator-() const noexcept
	{
		return NiPoint2(-x, -y);
	}

	constexpr NiPoint2& NiPoint2::operator+=(const NiPoint2& a_rhs) noexcept
	{
		x += a_rhs.x;
		y += a_rhs.y;
		return *this;
	}

	constexpr NiPoint2& NiPoint2::operator-=(const NiPoint2& a_rhs) noexcept
	{
		x -= a_rhs.x;
		y -= a_rhs.y;
		return *this;
	}

	constexpr NiPoint2& NiPoint2::operator*=(const NiPoint2& a_rhs) noexcept
	{
		x *= a_rhs.x;
		y *= a_rhs.y;
		return *this;
	}

	constexpr NiPoint2& NiPoint2::operator/=(const NiPoint2& a_rhs) noexcept
	{
		x /= a_rhs.x;
		y /= a_rhs.y;
		return *this;
	}

	constexpr NiPoint2& NiPoint2::operator+=(float a_scalar) noexcept
	{
		x += a_scalar;
		y += a_scalar;
		return *this;
	}

	constexpr NiPoint2& NiPoint2::operator-=(float a_scalar) noexcept
	{
		x -= a_scalar;
		y -= a_scalar;
		return *this;
	}

	constexpr NiPoint2& NiPoint2::operator*=(float a_scalar) noexcept
	{
		x *= a_scalar;
		y *= a_scalar;
		return *this;
	}

	constexpr NiPoint2& NiPoint2::operator/=(float a_scalar) noexcept
	{
		x /= a_scalar;
		y /= a_scalar;
		return *this;
	}

	inline const NiPoint2 NiPoint2::ZERO = { 0.0F, 0.0F };
	inline const NiPoint2 NiPoint2::UNIT = { 1.0F, 1.0F };
	inline const NiPoint2 NiPoint2::UNIT_X = { 1.0F, 0.0F };
	inline const NiPoint2 NiPoint2::UNIT_Y = { 0.0F, 1.0F };
}

template <>
struct std::formatter<RE::NiPoint2>
{
	template <class ParseContext>
	constexpr auto parse(ParseContext& a_ctx)
	{
		return a_ctx.begin();
	}

	template <class FormatContext>
	constexpr auto format(const RE::NiPoint2& a_point, FormatContext& a_ctx) const
	{
		return format_to(a_ctx.out(), "({}, {})", a_point.x, a_point.y);
	}
};
