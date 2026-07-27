#pragma once

#include "RE/H/hkBool.h"
#include "RE/H/hkReferencedObject.h"

namespace RE
{
	class hkSeekableStreamReader;

	class __declspec(novtable) hkStreamReader :
		public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkStreamReader };
		inline static constexpr auto VTABLE{ VTABLE::hkStreamReader };

		hkStreamReader() { REX::EMPLACE_VTABLE(this); }

		// add
		virtual hkBool                  IsOk() = 0;                                                                                        // 0x20 [04]
		virtual std::int32_t            Read([[maybe_unused]] void* a_buffer, [[maybe_unused]] std::int32_t a_numBytes) = 0;               // 0x28 [05]
		virtual std::int32_t            Skip([[maybe_unused]] std::int32_t a_numBytes) { return 0; }                                       // 0x30 [06]
		virtual std::int32_t            Peek([[maybe_unused]] void* a_buffer, [[maybe_unused]] std::int32_t a_numBytes) { return false; }  // 0x38 [07]
		virtual hkSeekableStreamReader* IsSeekTellSupported() { return nullptr; }                                                          // 0x40 [08]
	};
	static_assert(sizeof(hkStreamReader) == 0x10);
}
