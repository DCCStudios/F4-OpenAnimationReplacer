#pragma once

namespace RE
{
	class __declspec(novtable) BSWaterShaderMaterial :
		public BSShaderMaterial  // 000
	{
	public:
		static constexpr auto RTTI{ RTTI::BSWaterShaderMaterial };
		static constexpr auto VTABLE{ VTABLE::BSWaterShaderMaterial };

		// members
		NiPointer<NiTexture> staticReflectionMap;          // 038
		NiPointer<NiTexture> normalMap01;                  // 040
		NiPointer<NiTexture> normalMap02;                  // 048
		NiPointer<NiTexture> normalMap03;                  // 050
		NiColorA             shallowColor;                 // 058
		NiColorA             deepColor;                    // 068
		NiColorA             reflectionColor;              // 078
		NiColorA             underwaterFogColor;           // 088
		NiColorA             lightSiltColor;               // 090
		NiColorA             darkSiltcolor;                // 0A8
		NiColorA             varAmounts;                   // 0B8
		NiColorA             waterParams1;                 // 0C8
		NiColorA             waterParams2;                 // 0D8
		NiColorA             waterParams3;                 // 0E8
		NiColorA             waterParams4;                 // 0F8
		NiColorA             normalsScroll1;               // 108
		NiColorA             normalsScroll2;               // 118
		NiColorA             normalsScale;                 // 128
		NiColorA             normalsAmplitude;             // 138
		NiPlane              reflectionPlane;              // 148
		std::int32_t         texOffsetX;                   // 158
		std::int32_t         texOffsetY;                   // 15C
		bool                 updateConstants;              // 160
		bool                 useDefaultReflectionTexture;  // 161
		bool                 useSSR;                       // 162
	};
	static_assert(sizeof(BSWaterShaderMaterial) == 0x168);
}
