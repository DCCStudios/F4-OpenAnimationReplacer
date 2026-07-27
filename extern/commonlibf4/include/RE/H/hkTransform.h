#pragma once

#include "RE/H/hkRotation.h"

namespace RE
{
	class hkTransformf
	{
	public:
		constexpr hkTransformf() noexcept = default;

		constexpr hkTransformf(const hkRotationf& a_rotation, const hkVector4f& a_translation) noexcept :
			m_rotation(a_rotation), m_translation(a_translation)
		{}

		// members
		hkRotationf m_rotation;
		hkVector4f  m_translation;
	};

	using hkTransform = hkTransformf;
}
