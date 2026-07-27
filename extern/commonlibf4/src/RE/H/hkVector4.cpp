#include "RE/H/hkVector4.h"

namespace RE
{
	hkVector4f hkVector4f::GetNormalized() const
	{
		hkVector4f norm = *this;
		norm.Normalize();
		return norm;
	}

	float hkVector4f::Length() const
	{
		return std::sqrt(x * x + y * y + z * z);
	}

	hkVector4f& hkVector4f::Normalize()
	{
		float l = Length();
		if (l == 0) {
			this->x = 0;
			this->y = 0;
			this->z = 0;
			return *this;
		}

		this->x /= l;
		this->y /= l;
		this->z /= l;

		return *this;
	}
}
