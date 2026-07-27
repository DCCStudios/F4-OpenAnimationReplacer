#pragma once

#include "RE/H/hkOArchive.h"

namespace RE
{
	class __declspec(novtable) hkDisplaySerializeOStream :
		public hkOArchive
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkDisplaySerializeOStream };
		inline static constexpr auto VTABLE{ VTABLE::hkDisplaySerializeOStream };
	};
}
