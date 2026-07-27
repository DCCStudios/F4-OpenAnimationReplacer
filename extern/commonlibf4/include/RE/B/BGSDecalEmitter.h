#pragma once

#include "RE/N/NiPointer.h"

namespace RE
{
	class BGSImpactData;
	class BSTempEffectParticle;

	class BGSDecalEmitter
	{
	public:
		// members
		std::int32_t                    decalsToEmit;     // 00
		bool                            finished;         // 04
		BGSImpactData*                  decalImpactData;  // 08
		NiPointer<BSTempEffectParticle> particleData;     // 10
	};
	static_assert(sizeof(BGSDecalEmitter) == 0x18);
}
