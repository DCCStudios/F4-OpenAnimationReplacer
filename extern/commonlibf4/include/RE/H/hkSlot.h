#pragma once

#include "RE/H/hkPtrAndInt.h"

namespace RE
{
	class hkSlot
	{
	public:
		static constexpr auto RTTI{ RTTI::hkSlot };
		static constexpr auto VTABLE{ VTABLE::hkSlot };

		// add
		virtual ~hkSlot();                                             // 00
		virtual std::uint32_t matchMethod(const void*, std::int32_t);  // 01

		// members
		hkPtrAndInt<hkSlot, std::uint32_t, 3> m_next;    // 0x08
		void*                                 m_object;  // 0x10
	};
	static_assert(sizeof(hkSlot) == 0x18);
}
