#pragma once

namespace RE
{
	class __declspec(align(16)) hkVector4f
	{
	public:
		static const hkVector4f ZERO;

		constexpr hkVector4f() noexcept = default;
		constexpr hkVector4f(float a_x, float a_y, float a_z, float a_w) noexcept;

		constexpr float&       operator[](std::size_t a_pos) noexcept;
		constexpr const float& operator[](std::size_t a_pos) const noexcept;
		constexpr bool         operator==(const hkVector4f& a_rhs) const noexcept;
		constexpr bool         operator!=(const hkVector4f& a_rhs) const noexcept;
		constexpr bool         operator<(const hkVector4f& a_rhs) const noexcept;
		constexpr bool         operator>(const hkVector4f& a_rhs) const noexcept;
		constexpr hkVector4f   operator+(const hkVector4f& a_rhs) const noexcept;
		constexpr hkVector4f   operator-(const hkVector4f& a_rhs) const noexcept;
		constexpr hkVector4f   operator*(const hkVector4f& a_rhs) const noexcept;
		constexpr hkVector4f   operator/(const hkVector4f& a_rhs) const noexcept;
		constexpr hkVector4f   operator*(float a_scalar) const noexcept;
		constexpr hkVector4f   operator/(float a_scalar) const noexcept;
		constexpr hkVector4f   operator-() const noexcept;
		constexpr hkVector4f&  operator+=(const hkVector4f& a_rhs) noexcept;
		constexpr hkVector4f&  operator-=(const hkVector4f& a_rhs) noexcept;
		constexpr hkVector4f&  operator*=(const hkVector4f& a_rhs) noexcept;
		constexpr hkVector4f&  operator/=(const hkVector4f& a_rhs) noexcept;
		constexpr hkVector4f&  operator+=(float a_scalar) noexcept;
		constexpr hkVector4f&  operator-=(float a_scalar) noexcept;
		constexpr hkVector4f&  operator*=(float a_scalar) noexcept;
		constexpr hkVector4f&  operator/=(float a_scalar) noexcept;

		constexpr float Dot(const hkVector4f& v) const;

		hkVector4f  GetNormalized() const;
		float       Length() const;
		hkVector4f& Normalize();

		// members
		float x{ 0.0F };
		float y{ 0.0F };
		float z{ 0.0F };
		float w{ 0.0F };
	};
	static_assert(sizeof(hkVector4f) == 0x10);

	using hkVector4 = hkVector4f;
};

namespace RE
{
	inline const hkVector4f hkVector4f::ZERO = { 0.0F, 0.0F, 0.0F, 0.0F };

	constexpr hkVector4f::hkVector4f(float a_x, float a_y, float a_z, float a_w) noexcept :
		x(a_x), y(a_y), z(a_z), w(a_w)
	{}

	constexpr float& hkVector4f::operator[](std::size_t a_pos) noexcept
	{
		assert(a_pos < 4);
		return std::addressof(x)[a_pos];
	}

	constexpr const float& hkVector4f::operator[](std::size_t a_pos) const noexcept
	{
		assert(a_pos < 4);
		return std::addressof(x)[a_pos];
	}

	constexpr bool hkVector4f::operator==(const hkVector4f& a_rhs) const noexcept
	{
		return (x == a_rhs.x) && (y == a_rhs.y) && (z == a_rhs.z) && (w == a_rhs.w);
	}

	constexpr bool hkVector4f::operator!=(const hkVector4f& a_rhs) const noexcept
	{
		return !operator==(a_rhs);
	}

	constexpr bool hkVector4f::operator<(const hkVector4f& a_rhs) const noexcept
	{
		return std::tie(x, y, z, w) < std::tie(a_rhs.x, a_rhs.y, a_rhs.z, a_rhs.w);
	}

	constexpr bool hkVector4f::operator>(const hkVector4f& a_rhs) const noexcept
	{
		return std::tie(x, y, z, w) > std::tie(a_rhs.x, a_rhs.y, a_rhs.z, a_rhs.w);
	}

	constexpr hkVector4f hkVector4f::operator+(const hkVector4f& a_rhs) const noexcept
	{
		return hkVector4f(x + a_rhs.x, y + a_rhs.y, z + a_rhs.z, w + a_rhs.w);
	}

	constexpr hkVector4f hkVector4f::operator-(const hkVector4f& a_rhs) const noexcept
	{
		return hkVector4f(x - a_rhs.x, y - a_rhs.y, z - a_rhs.z, w - a_rhs.w);
	}

	constexpr hkVector4f hkVector4f::operator*(const hkVector4f& a_rhs) const noexcept
	{
		return hkVector4f(x * a_rhs.x, y * a_rhs.y, z * a_rhs.z, w * a_rhs.w);
	}

	constexpr hkVector4f hkVector4f::operator/(const hkVector4f& a_rhs) const noexcept
	{
		return hkVector4f(x / a_rhs.x, y / a_rhs.y, z / a_rhs.z, w / a_rhs.w);
	}

	constexpr hkVector4f hkVector4f::operator*(float a_scalar) const noexcept
	{
		return hkVector4f(x * a_scalar, y * a_scalar, z * a_scalar, w * a_scalar);
	}

	constexpr hkVector4f hkVector4f::operator/(float a_scalar) const noexcept
	{
		return operator*(1.0F / a_scalar);
	}

	constexpr hkVector4f hkVector4f::operator-() const noexcept
	{
		return hkVector4f(-x, -y, -z, -w);
	}

	constexpr hkVector4f& hkVector4f::operator+=(const hkVector4f& a_rhs) noexcept
	{
		x += a_rhs.x;
		y += a_rhs.y;
		z += a_rhs.z;
		w += a_rhs.w;
		return *this;
	}

	constexpr hkVector4f& hkVector4f::operator-=(const hkVector4f& a_rhs) noexcept
	{
		x -= a_rhs.x;
		y -= a_rhs.y;
		z -= a_rhs.z;
		w -= a_rhs.w;
		return *this;
	}

	constexpr hkVector4f& hkVector4f::operator*=(const hkVector4f& a_rhs) noexcept
	{
		x *= a_rhs.x;
		y *= a_rhs.y;
		z *= a_rhs.z;
		w *= a_rhs.w;
		return *this;
	}

	constexpr hkVector4f& hkVector4f::operator/=(const hkVector4f& a_rhs) noexcept
	{
		x /= a_rhs.x;
		y /= a_rhs.y;
		z /= a_rhs.z;
		w /= a_rhs.w;
		return *this;
	}

	constexpr hkVector4f& hkVector4f::operator+=(float a_scalar) noexcept
	{
		x += a_scalar;
		y += a_scalar;
		z += a_scalar;
		w += a_scalar;
		return *this;
	}

	constexpr hkVector4f& hkVector4f::operator-=(float a_scalar) noexcept
	{
		x -= a_scalar;
		y -= a_scalar;
		z -= a_scalar;
		w -= a_scalar;
		return *this;
	}

	constexpr hkVector4f& hkVector4f::operator*=(float a_scalar) noexcept
	{
		x *= a_scalar;
		y *= a_scalar;
		z *= a_scalar;
		w *= a_scalar;
		return *this;
	}

	constexpr hkVector4f& hkVector4f::operator/=(float a_scalar) noexcept
	{
		x /= a_scalar;
		y /= a_scalar;
		z /= a_scalar;
		w /= a_scalar;
		return *this;
	}

	constexpr float hkVector4f::Dot(const hkVector4f& v) const
	{
		return this->x * v.x + this->y * v.y + this->z * v.z;
	}
}
