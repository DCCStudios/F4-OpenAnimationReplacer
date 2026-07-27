#pragma once
#include "RE/T/TESGrass.h"

namespace RE
{
	class GrassParam;
	class NiPoint3;
	class TESObjectLAND;

	struct CreateGrassParams
	{
	public:
		std::int32_t                cellX;            // 00
		std::int32_t                cellY;            // 04
		std::int32_t                block;            // 08
		const TESObjectLAND*        land;             // 10
		const NiPoint3*             coord;            // 18
		const GrassParam*           grassParam;       // 20
		const float*                density;          // 28
		float                       heightLimit;      // 30
		TESGrass::GRASS_WATER_STATE waterState;       // 34
		float                       cellWaterHeight;  // 38
		std::uint32_t               minSlope;         // 3C
		std::uint32_t               maxSlope;         // 40
		std::uint32_t               randomSeed;       // 44
	};
	static_assert(sizeof(CreateGrassParams) == 0x48);
}
