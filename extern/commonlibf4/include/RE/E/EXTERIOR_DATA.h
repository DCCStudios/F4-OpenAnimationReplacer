#pragma once

namespace RE
{
	class BGSTerrainVisibilityData;

	class EXTERIOR_DATA
	{
	public:
		enum class LandHideFlag : std::uint8_t
		{
			kNone = 0,
			kQuad1 = 1 << 0,
			kQuad2 = 1 << 1,
			kQuad3 = 1 << 2,
			kQuad4 = 1 << 3
		};

		// members
		std::int32_t                cellX;          // 0x00
		std::int32_t                cellY;          // 0x04
		std::uint8_t*               maxHeightData;  // 0x08
		BGSTerrainVisibilityData*   lodVisData;     // 0x10
		float                       maxHeight;      // 0x18
		float                       worldX;         // 0x1C
		float                       worldY;         // 0x20
		REX::TEnumSet<LandHideFlag> landHideFlags;  // 0x24
	};
	static_assert(sizeof(EXTERIOR_DATA) == 0x28);
}
