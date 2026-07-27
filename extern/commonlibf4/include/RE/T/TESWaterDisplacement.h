#pragma once

namespace RE
{
	class __declspec(novtable) TESWaterDisplacement :
		public NiRefObject  // 00
	{
		enum class Flags
		{
			kDirty = 0x1
		};

		// members
		BSTSmartPointer<BSWaterShaderMaterial, BSTSmartPointerIntrusiveRefCount> waterMaterial;         // 10
		TESWaterForm*                                                            waterType;             // 18
		float                                                                    height;                // 20
		std::uint32_t                                                            stencilBits;           // 24
		std::uint8_t                                                             flags;                 // 28
		NiPointer<NiAVObject>                                                    displacementGeometry;  // 30
	};
	static_assert(sizeof(TESWaterDisplacement) == 0x38);
}
