#pragma once

#include "RE/N/NiTList.h"
#include "RE/N/NiTPointerAllocator.h"

namespace RE
{
	template <class T>
	class NiTPointerList : public NiTPointerListBase<NiTPointerAllocator<std::uintptr_t>, T>
	{
	public:
	};
	static_assert(sizeof(NiTPointerList<void*>) == 0x18);
}
