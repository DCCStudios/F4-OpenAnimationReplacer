#pragma once

#include "RE/N/NiMatrix3.h"
#include "RE/N/NiPoint3.h"

namespace RE
{
	class NiTransform
	{
	public:
		static const NiTransform ZERO;
		static const NiTransform IDENTITY;

		constexpr NiTransform() noexcept = default;
		constexpr NiTransform(const NiMatrix3& a_rotation, const NiPoint3& a_translation, const float a_scale = 1.0F) noexcept;

		constexpr bool        operator==(const NiTransform& a_rhs) const noexcept;
		constexpr bool        operator!=(const NiTransform& a_rhs) const noexcept;
		constexpr NiTransform operator*(const NiTransform& a_rhs) const noexcept;
		constexpr NiPoint3    operator*(const NiPoint3& a_rhs) const noexcept;

		constexpr NiTransform  Invert() const noexcept;
		constexpr NiTransform& MakeIdentity() noexcept;
		constexpr NiTransform& MakeInvert() noexcept;

		// members
		NiMatrix3 rotate;         // 00
		NiPoint3  translate;      // 30
		float     scale{ 1.0F };  // 3C
	};
	static_assert(sizeof(NiTransform) == 0x40);
}

namespace RE
{
	inline const NiTransform NiTransform::ZERO = { NiMatrix3::ZERO, NiPoint3::ZERO, 0.0F };
	inline const NiTransform NiTransform::IDENTITY = { NiMatrix3::IDENTITY, NiPoint3::ZERO, 1.0F };

	constexpr NiTransform::NiTransform(const NiMatrix3& a_rotation, const NiPoint3& a_translation, const float a_scale) noexcept :
		rotate(a_rotation), translate(a_translation), scale(a_scale)
	{}

	constexpr bool NiTransform::operator==(const NiTransform& a_rhs) const noexcept
	{
		return (rotate == a_rhs.rotate) && (translate == a_rhs.translate) && (scale == a_rhs.scale);
	}

	constexpr bool NiTransform::operator!=(const NiTransform& a_rhs) const noexcept
	{
		return !operator==(a_rhs);
	}

	constexpr NiTransform NiTransform::operator*(const NiTransform& a_rhs) const noexcept
	{
		return NiTransform(rotate * a_rhs.rotate, translate + (rotate * a_rhs.translate) * scale, scale * a_rhs.scale);
	}

	constexpr NiPoint3 NiTransform::operator*(const NiPoint3& a_rhs) const noexcept
	{
		return NiPoint3(((rotate * a_rhs) * scale) + translate);
	}

	constexpr NiTransform NiTransform::Invert() const noexcept
	{
		NiTransform transform(*this);
		return transform.MakeInvert();
	}

	constexpr NiTransform& NiTransform::MakeIdentity() noexcept
	{
		rotate = NiMatrix3::IDENTITY;
		translate = NiPoint3::ZERO;
		scale = 1.0F;
		return *this;
	}

	constexpr NiTransform& NiTransform::MakeInvert() noexcept
	{
		rotate = rotate.Transpose();
		scale = 1.0f / scale;
		translate = (rotate * -translate) * scale;
		return *this;
	}
}

template <>
struct std::formatter<RE::NiTransform>
{
	template <class ParseContext>
	constexpr auto parse(ParseContext& a_ctx)
	{
		return a_ctx.begin();
	}

	template <class FormatContext>
	constexpr auto format(const RE::NiTransform& a_transform, FormatContext& a_ctx) const
	{
		return format_to(a_ctx.out(), "<{}, {}>", a_transform.rotate, a_transform.translate, a_transform.scale);
	}
};
