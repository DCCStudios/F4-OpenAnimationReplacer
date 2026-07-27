#pragma once

#include "RE/H/hkBool.h"

namespace RE
{
	class hkDebugDisplayHandler;
	class hkDisplaySerializeIStream;
	class hkDisplaySerializeOStream;
	class hkProcessHandler;

	class __declspec(novtable) hkProcess
	{
	public:
		inline static constexpr auto RTTI{ RTTI::hkProcess };
		inline static constexpr auto VTABLE{ VTABLE::hkProcess };

		hkProcess(hkBool a_selectable) :
			m_selectable(a_selectable)
		{
			REX::EMPLACE_VTABLE(this);
		}

		virtual ~hkProcess() = default;  // 0x00 [00]

		// add
		virtual void         Unk01() = 0;                                                                                                        // 0x08 [01]
		virtual std::int64_t GetType() { return 0; }                                                                                             // 0x10 [02]
		virtual void         GetConsumableCommands([[maybe_unused]] std::uint8_t** a_commands, [[maybe_unused]] std::int32_t* a_numCommands) {}  // 0x18 [03]
		virtual void         ConsumeCommand([[maybe_unused]] std::uint8_t a_command) {}                                                          // 0x20 [04]
		virtual bool         IsOk() { return false; }                                                                                            // 0x28 [05]
		virtual void         Init() {}                                                                                                           //
		virtual void         Step([[maybe_unused]] float a_frameTimeInMs) {}                                                                     //

		// members
		hkBool                     m_selectable;      // 0x08
		std::byte                  m_pad09[0x7];      // 0x09
		hkDisplaySerializeIStream* m_inStream;        // 0x10
		hkDisplaySerializeOStream* m_outStream;       // 0x18
		hkDebugDisplayHandler*     m_displayHandler;  // 0x20
		hkProcessHandler*          m_processHandler;  // 0x28
	};
	static_assert(sizeof(hkProcess) == 0x30);
}
