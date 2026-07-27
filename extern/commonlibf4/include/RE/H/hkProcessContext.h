#pragma once

#include "RE/H/hkArray.h"

namespace RE
{
	class hkVisualDebugger;

	class hkProcessContext
	{
	public:
		hkProcessContext() = default;

		virtual ~hkProcessContext() = default;  // 0x00 [00]

		// add
		virtual const char* GetType() = 0;  // 0x08 [01]

		virtual void SetOwner(hkVisualDebugger* a_owner)  // 0x10 [02]
		{
			m_owner = a_owner;
		}

		// members
		hkVisualDebugger*              m_owner{ nullptr };     // 0x08
		hkInplaceArray<const char*, 6> m_monitorStreamBegins;  // 0x10
		hkInplaceArray<const char*, 6> m_monitorStreamEnds;    // 0x50
	};
	static_assert(sizeof(hkProcessContext) == 0x90);
}
