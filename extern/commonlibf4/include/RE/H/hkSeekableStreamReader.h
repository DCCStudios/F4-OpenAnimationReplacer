#pragma once

#include "RE/H/hkStreamReader.h"

namespace RE
{
	class __declspec(novtable) hkSeekableStreamReader :
		public hkStreamReader
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkSeekableStreamReader };
		inline static constexpr auto VTABLE{ VTABLE::hkSeekableStreamReader };

		enum class SeekWhence : std::int32_t
		{
			kSet = 0,
			kCur = 1,
			kEnd = 2
		};

		hkSeekableStreamReader() { REX::EMPLACE_VTABLE(this); }

		// add
		virtual hkResult     Seek([[maybe_unused]] std::int32_t a_numBytes, [[maybe_unused]] SeekWhence a_whence) = 0;  // 0x48 [09]
		virtual std::int32_t Tell() = 0;                                                                                // 0x50 [10]
	};
	static_assert(sizeof(hkSeekableStreamReader) == 0x10);
}
