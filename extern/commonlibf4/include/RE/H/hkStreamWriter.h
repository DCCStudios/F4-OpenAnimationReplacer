#pragma once

#include "RE/H/hkBool.h"
#include "RE/H/hkReferencedObject.h"
#include "RE/H/hkResult.h"
#include "RE/H/hkSeekableStreamReader.h"

namespace RE
{
	class __declspec(novtable) hkStreamWriter :
		public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkStreamWriter };
		inline static constexpr auto VTABLE{ VTABLE::hkStreamWriter };

		using SeekWhence = hkSeekableStreamReader::SeekWhence;

		hkStreamWriter() { REX::EMPLACE_VTABLE(this); }

		// add
		virtual hkBool       IsOk() = 0;                                                                                                        // 0x20 [04]
		virtual std::int32_t Write(const void* a_buffer, std::int32_t a_numBytes) = 0;                                                          // 0x28 [05]
		virtual void         Flush() {}                                                                                                         // 0x30 [06]
		virtual hkBool       SeekTellSupported() { return false; }                                                                              // 0x38 [07]
		virtual hkResult     Seek([[maybe_unused]] std::int32_t a_offset, [[maybe_unused]] SeekWhence a_whence) { return hkResult::kFailure; }  // 0x40 [08]
		virtual std::int32_t Tell() { return 0xFFFFFFFF; }                                                                                      // 0x48 [09]
		virtual hkBool       IsBuffered() { return false; }                                                                                     // 0x50 [10]
	};
}
