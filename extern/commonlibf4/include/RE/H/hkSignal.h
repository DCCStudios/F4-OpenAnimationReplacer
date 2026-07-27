#pragma once

#include "RE/H/hkPtrAndInt.h"
#include "RE/H/hkSlot.h"

namespace RE
{
	class hkSignal
	{
	public:
		// members
		hkPtrAndInt<hkSlot, std::uint32_t, 3> m_slots;  // 00
	};
	static_assert(sizeof(hkSignal) == 0x08);

	struct hkSignal0 : hkSignal
	{};

	template <class P0>
	struct hkSignal1 : hkSignal
	{};

	template <class P0, class P1>
	struct hkSignal2 : hkSignal
	{};

	template <class P0, class P1, class P2>
	struct hkSignal3 : hkSignal
	{};

	template <class P0, class P1, class P2, class P3>
	struct hkSignal4 : hkSignal
	{};

	template <class P0, class P1, class P2, class P3, class P4>
	struct hkSignal5 : hkSignal
	{};
}
