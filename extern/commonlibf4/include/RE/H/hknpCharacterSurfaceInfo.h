#pragma once

#include "RE/H/hkBool.h"
#include "RE/H/hkEnum.h"
#include "RE/H/hkVector4.h"

namespace RE
{
	class hknpCharacterSurfaceInfo
	{
	public:
		enum class SupportedState
		{
			kUnsupported,
			kSliding,
			kSupported
		};

		// members
		hkBool                               m_isSurfaceDynamic;       // 0x00
		hkEnum<SupportedState, std::uint8_t> m_supportedState;         // 0x01
		std::byte                            m_pad02[0x02];            // 0x02
		float                                m_surfaceDistanceExcess;  // 0x04
		std::byte                            m_pad08[8];               // 0x08
		hkVector4f                           m_surfaceNormal;          // 0x10
		hkVector4f                           m_surfaceVelocity;        // 0x20
	};
	static_assert(sizeof(hknpCharacterSurfaceInfo) == 0x30);
}
