#pragma once

#include "RE/H/hkArray.h"
#include "RE/H/hkBitField.h"
#include "RE/H/hkBool.h"
#include "RE/H/hkFlags.h"
#include "RE/H/hkMap.h"
#include "RE/H/hknpBodyId.h"

namespace RE
{
	class hkAabb16;
	class hknpBody;
	class hknpMotionManager;
	class hknpWorld;
	class hkStringPtr;

	class hknpBodyManager
	{
	public:
		enum class ScheduledBodyFlagsEnum;

		class BodyIterator
		{
		public:
			// members
			const hknpBodyManager* m_bodyManager;  // 0x00
			std::int32_t           m_bodyIndex;    // 0x08
		};
		static_assert(sizeof(BodyIterator) == 0x10);

		class PropertyBuffer
		{
		public:
			// members
			std::int32_t m_bufferSize;    // 0x00
			std::int32_t m_propertySize;  // 0x04
			hkBitField   m_occupancy;     // 0x08
			void*        m_properties;    // 0x20
		};
		static_assert(sizeof(PropertyBuffer) == 0x28);

		class ScheduledBodyChange
		{
		public:
			// members
			hknpBodyId                                     m_bodyId;              // 0x00
			hkFlags<ScheduledBodyFlagsEnum, std::uint16_t> m_scheduledBodyFlags;  // 0x04
			std::uint32_t                                  m_pendingAddIndex;     // 0x08
		};
		static_assert(sizeof(ScheduledBodyChange) == 0x0C);

		// members
		hknpWorld*                            m_world;                       // 00
		hknpMotionManager*                    m_motionManager;               // 08
		hkArray<hknpBody>                     m_bodies;                      // 10
		hkBool                                m_bodyBufferIsUserOwned;       // 20
		hknpBodyId                            m_firstFreeBodyId;             // 24
		hkArray<hkAabb16>                     m_previousAabbs;               // 28
		hkArray<hkStringPtr>                  m_bodyNames;                   // 38
		hkMap<std::uint16_t, PropertyBuffer*> m_propertyMap;                 // 48
		std::uint32_t                         m_numAllocatedBodies;          // 58
		std::uint32_t                         m_numMarkedBodies;             // 5C
		std::uint32_t                         m_peakBodyIndex;               // 60
		hkArray<hknpBodyId>                   m_activeBodyIds;               // 68
		hkArray<std::uint8_t>                 m_bodyIdToCellIndexMap;        // 78
		hkArray<ScheduledBodyChange>          m_scheduledBodyChanges;        // 88
		hkArray<std::uint32_t>                m_scheduledBodyChangeIndices;  // 98
		hkArray<hknpBodyId>                   m_bodiesToAddAsActive;         // A8
		hkArray<hknpBodyId>                   m_bodiesToAddAsInactive;       // B8
	};
	static_assert(sizeof(hknpBodyManager) == 0xC8);
}
