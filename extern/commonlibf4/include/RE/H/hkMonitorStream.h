#pragma once

#include "RE/H/hkBool.h"
#include "RE/H/hkPadSpu.h"

namespace RE
{
	class hkMonitorStream
	{
	public:
		inline static hkMonitorStream* GetSingleton()
		{
			static REL::Relocation<std::uint32_t*> slotID{ ID::hkMonitorStream::tlsSlotID };
			return static_cast<hkMonitorStream*>(REX::W32::TlsGetValue(*slotID));
		}

		void Clear()
		{
			using func_t = decltype(&hkMonitorStream::Clear);
			static REL::Relocation<func_t> func{ ID::hkMonitorStream::Clear };
			func(this);
		}

		void Reset()
		{
			m_end = m_start;
		}

		void Resize(const std::int32_t a_newSize)
		{
			using func_t = decltype(&hkMonitorStream::Resize);
			static REL::Relocation<func_t> func{ ID::hkMonitorStream::Resize };
			func(this, a_newSize);
		}

		// members
		hkPadSpu<char*> m_start;                       // 0x00
		hkPadSpu<char*> m_end;                         // 0x08
		hkPadSpu<char*> m_capacity;                    // 0x10
		hkPadSpu<char*> m_capacityMinus16;             // 0x18
		hkBool          m_isBufferAllocatedOnTheHeap;  // 0x20
	};
	static_assert(sizeof(hkMonitorStream) == 0x28);
}
