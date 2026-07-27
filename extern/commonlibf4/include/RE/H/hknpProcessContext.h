#pragma once

#include "RE/H/hkProcessContext.h"
#include "RE/H/hkReferencedObject.h"
#include "RE/H/hknpDefaultViewerColorScheme.h"

namespace RE
{
	class hknpProcessContextListener;
	class hknpWorld;
	class hkTaskQueue;

	class hknpProcessContext :
		public hkReferencedObject,
		public hkProcessContext
	{
	public:
		hknpProcessContext() = default;

		virtual const char* GetType() override
		{
			return "Physics";
		}

		void AddWorld(hknpWorld* a_world)
		{
			for (auto world : m_worlds) {
				if (world == a_world)
					return;
			}

			m_worlds.push_back(a_world);
		}

		void SetColorScheme(hknpViewerColorScheme* a_colorScheme)
		{
			if (a_colorScheme)
				m_colorScheme = a_colorScheme;
			else
				m_colorScheme = &m_defaultColorScheme;
		}

		// members
		hkArray<hknpWorld*>                  m_worlds;                                // 0xA0
		hkArray<hknpProcessContextListener*> m_addListeners;                          // 0xB0
		hknpViewerColorScheme*               m_colorScheme{ &m_defaultColorScheme };  // 0xC0
		hknpDefaultViewerColorScheme         m_defaultColorScheme;                    // 0xC8
		hkTaskQueue*                         m_taskQueue{ nullptr };                  // 0xE8
	};
	static_assert(sizeof(hknpProcessContext) == 0xF0);
}
