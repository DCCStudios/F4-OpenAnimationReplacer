#pragma once

#include "RE/H/hkArray.h"
#include "RE/H/hkBool.h"
#include "RE/H/hknpMotionId.h"

namespace RE
{
	class hknpBodyManager;
	class hknpMotion;

	class hknpMotionManager
	{
	public:
		class CellData
		{
		public:
			// members
			hkArray<hknpMotionId> m_solverIdToMotionId;  // 0x00
		};
		static_assert(sizeof(CellData) == 0x10);

		// members
		hknpBodyManager*    m_bodyManager;              // 0x00
		hkArray<hknpMotion> m_motions;                  // 0x08
		hkBool              m_motionBufferIsUserOwned;  // 0x18
		hknpMotionId        m_firstFreeMotionId;        // 0x1C
		hknpMotionId        m_firstMarkedMotionId;      // 0x20
		std::uint32_t       m_numAllocatedMotions;      // 0x24
		std::uint32_t       m_numMarkedMotions;         // 0x28
		std::uint32_t       m_peakMotionIndex;          // 0x2C
		hkArray<CellData>   m_activeMotionGrid;         // 0x30
		hkBool              m_isLocked;                 // 0x40
	};
	static_assert(sizeof(hknpMotionManager) == 0x48);
}
