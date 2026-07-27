#pragma once

#include "RE/H/hkBool.h"
#include "RE/H/hkRefPtr.h"
#include "RE/H/hkReferencedObject.h"
#include "RE/H/hkStreamWriter.h"

namespace RE
{
	class __declspec(novtable) hkOArchive :
		public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkOArchive };
		inline static constexpr auto VTABLE{ VTABLE::hkOArchive };

		bool IsOk() const
		{
			return m_writer->IsOk();
		}

		// members
		hkRefPtr<hkStreamWriter> m_writer;    // 0x10
		hkBool                   m_byteSwap;  // 0x18
		std::byte                m_pad19[7];  // 0x19
	};
	static_assert(sizeof(hkOArchive) == 0x20);
}
