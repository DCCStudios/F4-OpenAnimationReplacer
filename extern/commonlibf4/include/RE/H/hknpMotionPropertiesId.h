#pragma once

#include "RE/H/hkHandle.h"

namespace RE
{
	class hknpMotionPropertiesIdBaseDiscriminant
	{
	public:
	};
	static_assert(std::is_empty_v<hknpMotionPropertiesIdBaseDiscriminant>);

	class hknpMotionPropertiesId :
		public hkHandle<std::uint16_t, 65535, hknpMotionPropertiesIdBaseDiscriminant>
	{
	public:
		enum class Preset : std::uint32_t
		{
			kStatic = 0,
			kDynamic = 1,
			kKeyframed = 2,
			kFrozen = 3,
			kDebris = 4,
			kCount = 5,
			kInvalid = 0xFFFF
		};
	};
	static_assert(sizeof(hknpMotionPropertiesId) == 0x02);
}
