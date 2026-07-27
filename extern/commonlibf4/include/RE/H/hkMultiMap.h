#pragma once

#include "RE/H/hkContainerHeapAllocator.h"

namespace RE
{
	template <class K>
	struct hkMultiMapOperations
	{};

	template <>
	struct hkMultiMapOperations<std::int32_t>
	{};

	template <class K, class V = K, class O = hkMultiMapOperations<K>, class A = hkContainerHeapAllocator>
	class hkMultiMap
	{
	public:
		struct Pair
		{
			K key;
			V value;
		};

		// members
		Pair*        elem;       // 0x00
		std::int32_t elemCount;  // 0x08 - high bits are flags
		std::int32_t hashMod;    // 0x10 - capacity - 1
	};
	static_assert(sizeof(hkMultiMap<int>) == 0x10);
}
