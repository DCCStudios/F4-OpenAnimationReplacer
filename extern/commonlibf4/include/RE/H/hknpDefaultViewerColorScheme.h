#pragma once

#include "RE/H/hknpViewerColorScheme.h"

namespace RE
{
	class hknpDefaultViewerColorScheme :
		public hknpViewerColorScheme
	{
	public:
		hknpDefaultViewerColorScheme() = default;

		virtual std::uint32_t GetBodyColor(const hknpWorld*, hknpBodyId, hknpViewer*) override
		{
			return staticBodyColor;  // TODO: Get color based on body flags
		}

		virtual bool IsBodyVisible(const hknpWorld*, hknpBodyId, hknpViewer*) override
		{
			return true;  // TODO: err...?
		}

		// members
		std::uint32_t staticBodyColor{ 0xFF999999 };     // 0x08
		std::uint32_t dynamicBodyColor{ 0xFFFFB300 };    // 0x0C
		std::uint32_t keyframedBodyColor{ 0xFF800080 };  // 0x10
		std::uint32_t triggerColor{ 0x4400FF00 };        // 0x14
		std::uint32_t dontCollideColor{ 0x66FFFF00 };    // 0x18
	};
	static_assert(sizeof(hknpDefaultViewerColorScheme) == 0x20);
}
