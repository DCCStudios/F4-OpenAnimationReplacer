#pragma once

#include "RE/B/BSTArray.h"
#include "RE/C/CHAR_NORM.h"
#include "RE/H/hkRefPtr.h"
#include "RE/N/NiPointer.h"
#include "RE/O/OBJ_LAND.h"
#include "RE/T/TESChildCell.h"
#include "RE/T/TESForm.h"

namespace RE
{
	class QueuedFile;

	class __declspec(novtable) TESObjectLAND :
		public TESForm,      // 00
		public TESChildCell  // 20
	{
	public:
		static constexpr auto RTTI{ RTTI::TESObjectLAND };
		static constexpr auto VTABLE{ VTABLE::TESObjectLAND };
		static constexpr auto FORM_ID{ ENUM_FORM_ID::kLAND };

		enum class COORD_DATA_FLAGS
		{
			kVertexCoord = 0x1,
			kForceToVertex = 02,
			kCalcNearest = 0x4,
			kBlockOnly = 0x8,
			kDefaults = 0x5
		};

		class LoadedLandData
		{
		public:
			// members
			NiNode*                                                    mesh[4];                    // 0000
			float                                                      heights[1156];              // 0020
			std::uint8_t                                               percentArrays[4][289][12];  // 1230
			std::uint8_t                                               colors[4][289][3];          // 4860
			CHAR_NORM                                                  normals[4][289];            // 55EC
			BSTArray<hkRefPtr<hknpShape const>, BSTArrayHeapAllocator> saveLoadHFData;             // 6378
			NiPointer<BSTriShape>                                      geom[4];                    // 6390
			NiPointer<BSTriShape>                                      border;                     // 63B0
			NiPoint2                                                   heightExtends;              // 63B8
			TESLandTexture*                                            defQuadTexture[4];          // 63C0
			TESLandTexture*                                            quadTextureArray[4][12];    // 63E0
			std::int32_t                                               cellX;                      // 6560
			std::int32_t                                               cellY;                      // 6564
			float                                                      baseHeight;                 // 6568
			LoadedLandData*                                            next;                       // 6570
		};
		static_assert(sizeof(LoadedLandData) == 0x6578);

		// members
		OBJ_LAND              data;            // 28
		TESObjectCELL*        parentCell;      // 30
		NiPointer<QueuedFile> queuedTextures;  // 38
		LoadedLandData*       loadedData;      // 40
	};
	static_assert(sizeof(TESObjectLAND) == 0x48);
}
