#pragma once

#include "RE/H/hkProcess.h"
#include "RE/H/hkReferencedObject.h"
#include "RE/H/hknpBodyId.h"
#include "RE/H/hknpProcessContextListener.h"

namespace RE
{
	class hknpProcessContext;
	class hknpWorld;

	class hknpViewer :
		public hkReferencedObject,
		public hkProcess,
		public hknpProcessContextListener
	{
	public:
		// add
		virtual void OnSetViewerSpecificBody() {}  // 0x20 [04]

		// members
		hknpProcessContext* m_context;                    // 0x48
		hknpBodyId          m_selectedBody;               // 0x50
		std::byte           m_pad54[0x4];                 // 0x54
		const hknpWorld*    m_worldForViewerSpecifcBody;  // 0x58
	};
	static_assert(sizeof(hknpViewer) == 0x60);
}
