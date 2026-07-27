#pragma once

namespace RE
{
	class GrassParam
	{
	public:
		// members
		const char*   geomFilename;     // 00
		std::uint32_t grassID;          // 08
		float         positionRange;    // 0C
		float         heightRange;      // 10
		float         colorRange;       // 14
		float         waveOffsetRange;  // 18
		float         wavePeriod;       // 1C
		bool          vertexLighting;   // 20
		bool          uniformScaling;   // 21
		bool          fitToSlope;       // 22
		bool          wind;             // 23
	};
	static_assert(sizeof(GrassParam) == 0x28);
}
