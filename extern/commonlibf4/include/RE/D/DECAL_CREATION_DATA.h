#pragma once

#include "RE/B/BSPointerHandle.h"
#include "RE/N/NiColor.h"
#include "RE/N/NiMatrix3.h"
#include "RE/N/NiPoint3.h"
#include "RE/N/NiPointer.h"

namespace RE
{
	class BGSTextureSet;
	class NiAVObject;
	class NiNode;
	class TESObjectCELL;

	class DECAL_CREATION_DATA
	{
	public:
		// members
		NiMatrix3             projection;                  // 00
		NiPoint3A             origin;                      // 30
		NiPoint3A             direction;                   // 40
		NiPoint3A             surfaceNormal;               // 50
		ObjectRefHandle       objRef;                      // 60
		NiPointer<NiAVObject> avObj;                       // 68
		NiNode*               clone;                       // 70
		BGSTextureSet*        texSet;                      // 78
		BGSTextureSet*        texSet2;                     // 80
		std::int32_t          index;                       // 88
		float                 decalWidth;                  // 8C
		float                 decalHeight;                 // 90
		float                 decalDepth;                  // 94
		TESObjectCELL*        parentCell;                  // 98
		float                 parallaxOcclusionScale;      // A0
		NiNode*               skinnedDecalNode;            // A8
		float                 specular;                    // B0
		float                 decalEpsilon;                // B4
		float                 placementRadius;             // 0B8
		NiColor               vertexColor;                 // BC
		std::uint32_t         bodyParts;                   // C8
		std::uint8_t          subTex;                      // CC
		bool                  permanent;                   // CD
		bool                  parallaxOcclusionShadows;    // CE
		bool                  alphaTesting;                // CF
		bool                  alphaBlending;               // D0
		bool                  multiplicativeBlending;      // D1
		std::uint8_t          parallaxOcclusionMaxPasses;  // D2
		bool                  modelSpace;                  // D3
		bool                  forceFade;                   // D4
		bool                  twoSided;                    // D5
		bool                  preprocessed;                // D6
		bool                  manualSaveLoad;              // D7
		std::uint32_t         sceneGraphIndex;             // D8
		float                 decalLifetime;               // DC
		bool                  dynamic;                     // E0
		bool                  useGbufferNormal;            // E1
	};
	static_assert(sizeof(DECAL_CREATION_DATA) == 0xF0);
}
