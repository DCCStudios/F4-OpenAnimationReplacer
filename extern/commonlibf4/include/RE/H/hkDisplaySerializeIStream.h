#pragma once

#include "RE/H/hkIArchive.h"

namespace RE
{
	class __declspec(novtable) hkDisplaySerializeIStream :
		public hkIArchive
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkDisplaySerializeIStream };
		inline static constexpr auto VTABLE{ VTABLE::hkDisplaySerializeIStream };
	};
}
