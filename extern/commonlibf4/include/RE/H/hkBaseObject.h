#pragma once

namespace RE
{
	class __declspec(novtable) hkBaseObject
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkBaseObject };
		inline static constexpr auto VTABLE{ VTABLE::hkBaseObject };

		hkBaseObject() { REX::EMPLACE_VTABLE(this); }

		// add
		virtual ~hkBaseObject(){};                          // 0x00 [00]
		virtual void __first_virtual_table_function__() {}  // 0x08 [01]
	};
	static_assert(sizeof(hkBaseObject) == 0x08);
}
