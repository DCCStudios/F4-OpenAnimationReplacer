#pragma once

#include "RE/H/hkBool.h"
#include "RE/H/hkRefPtr.h"
#include "RE/H/hkReferencedObject.h"
#include "RE/H/hkStreamReader.h"

namespace RE
{
	class __declspec(novtable) hkIArchive :
		public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkIArchive };
		inline static constexpr auto VTABLE{ VTABLE::hkIArchive };

		bool IsOk() const
		{
			return m_reader->IsOk();
		}

		// members
		hkRefPtr<hkStreamReader> m_reader;    // 0x10
		hkBool                   m_byteSwap;  // 0x18
		std::byte                m_pad19[7];  // 0x19
	};
	static_assert(sizeof(hkIArchive) == 0x20);
}
