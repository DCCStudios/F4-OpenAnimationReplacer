#pragma once

#include "RE/B/BSExtraData.h"

namespace RE
{
	class __declspec(novtable) ExtraAmmo :
		public BSExtraData  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::ExtraAmmo };
		static constexpr auto VTABLE{ VTABLE::ExtraAmmo };
		static constexpr auto TYPE{ EXTRA_DATA_TYPE::kAmmo };

		// members
		std::uint32_t count;  // 18
	};
	static_assert(sizeof(ExtraAmmo) == 0x20);
}
