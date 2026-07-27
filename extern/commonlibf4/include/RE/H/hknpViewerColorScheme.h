#pragma once

#include "RE/H/hknpBodyId.h"

namespace RE
{
	class hknpWorld;
	class hknpViewer;

	class hknpViewerColorScheme
	{
	public:
		virtual ~hknpViewerColorScheme() = default;  // 0x00 [00]

		// add
		virtual std::uint32_t GetBodyColor(const hknpWorld* a_world, hknpBodyId a_bodyId, hknpViewer* a_viewer) = 0;   // 0x08 [01]
		virtual bool          IsBodyVisible(const hknpWorld* a_world, hknpBodyId a_bodyId, hknpViewer* a_viewer) = 0;  // 0x10 [02]
	};
	static_assert(sizeof(hknpViewerColorScheme) == 0x08);
}
