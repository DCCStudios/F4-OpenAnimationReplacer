#pragma once

#include "RE/H/hkArray.h"
#include "RE/H/hkBool.h"
#include "RE/H/hkFlags.h"
#include "RE/H/hknpBodyId.h"
#include "RE/H/hknpShapeSignals.h"

namespace RE
{
	class hknpShape;

	class hknpShapeManager
	{
	public:
		class MutableShapeInfo
		{
		public:
			// members
			hknpShapeManager*                                          m_shapeManager;  // 0x00
			const hknpShape*                                           m_shape;         // 0x08
			hkArray<hknpBodyId>                                        m_bodyIds;       // 0x10
			hkFlags<hknpShapeSignals::MutationFlagsEnum, std::uint8_t> m_mutations;     // 0x20
		};
		static_assert(sizeof(MutableShapeInfo) == 0x28);

		// members
		hkArray<MutableShapeInfo*> m_mutableShapeInfos;      // 0x00
		hkArray<MutableShapeInfo*> m_freeMutableShapeInfos;  // 0x10
		hkBool                     m_isAnyShapeMutated;      // 0x20
	};
	static_assert(sizeof(hknpShapeManager) == 0x28);
}
