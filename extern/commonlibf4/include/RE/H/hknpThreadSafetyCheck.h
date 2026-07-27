#pragma once

#include "RE/H/hkBool.h"
#include "RE/H/hkCriticalSection.h"

namespace RE
{
	class hknpThreadSafetyCheck
	{
	public:
		class State
		{
		public:
			// members
			std::uint32_t m_numReaders: 5;
			std::uint32_t m_numWriters: 3;
			std::uint32_t m_writerThreadId: 24;
		};
		static_assert(sizeof(State) == 0x04);

		// members
		State             m_state;            // 0x00
		hkCriticalSection m_criticalSection;  // 0x08
		hkBool            m_enabled;          // 0x30
	};
	static_assert(sizeof(hknpThreadSafetyCheck) == 0x38);
}
