#include "RE/N/NiPoint3.h"

namespace RE
{
	float NiPoint3::GetDistance(const NiPoint3& a_point) const noexcept
	{
		// std math functions are not constexpr yet
		return std::sqrtf(GetSquaredDistance(a_point));
	}

	float NiPoint3::GetZAngleFromVector() const
	{
		using func_t = decltype(&NiPoint3::GetZAngleFromVector);
		static REL::Relocation<func_t> func{ ID::NiPoint3::GetZAngleFromVector };
		return func(this);
	}

	float NiPoint3::Length() const noexcept
	{
		return std::sqrtf(x * x + y * y + z * z);
	}

	NiPoint3 NiPoint3::UnitCross(const NiPoint3& a_point) const noexcept
	{
		auto cross = Cross(a_point);
		cross.Unitize();
		return cross;
	}

	float NiPoint3::Unitize() noexcept
	{
		auto length = Length();
		if (length == 1.f) {
			return length;
		} else if (length > FLT_EPSILON) {
			operator/=(length);
		} else {
			x = 0.0;
			y = 0.0;
			z = 0.0;
			length = 0.0;
		}
		return length;
	}
}
