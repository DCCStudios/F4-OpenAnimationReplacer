#pragma once

namespace RE
{
	class NiColor
	{
	public:
		constexpr NiColor() = default;

		constexpr NiColor(const float a_red, const float a_green, const float a_blue) :
			r(a_red), g(a_green), b(a_blue)
		{}

		// members
		float r{ 0 };  // 0x0
		float g{ 0 };  // 0x4
		float b{ 0 };  // 0x8
	};
	static_assert(sizeof(NiColor) == 0xC);

	class NiColorA
	{
	public:
		constexpr NiColorA() = default;

		constexpr NiColorA(const float a_red, const float a_green, const float a_blue, const float a_alpha) :
			r(a_red), g(a_green), b(a_blue), a(a_alpha)
		{}

		constexpr NiColorA(const NiColor& a_color) :
			r(a_color.r), g(a_color.g), b(a_color.b)
		{}

		// members
		float r{ 0 };  // 0x0
		float g{ 0 };  // 0x4
		float b{ 0 };  // 0x8
		float a{ 0 };  // 0xC
	};
	static_assert(sizeof(NiColorA) == 0x10);
}
