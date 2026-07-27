#pragma once

namespace RE
{
	class BSCubeMapCamera;
	class CubeMapSide;

	class __declspec(novtable) TESWaterReflections :
		public NiRefObject  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::TESWaterReflections };
		static constexpr auto VTABLE{ VTABLE::TESWaterReflections };

		enum class Flags
		{
			kDirty = 0x0,
			kStaticCubemap = 0x2,
			kDynamicCubemap = 0x4,
			kInterior = 0x8,
			kSilhouette = 0x10,
			kLODScene = 0x20,
			kFullScene = 0x40,
			kLand = 0x80,
			kSky = 0x100,
			kExplosions = 0x200,
			kSelective = 0x400,
			kDontUpdate = 0x800,
			kMapMode = 0x1000
		};

		// members
		std::uint8_t                                                             flags;                  // 10
		NiPlane                                                                  reflectWaterPlane;      // 14
		NiPointer<BSCubeMapCamera>                                               cubeMapCamera;          // 28
		NiPointer<BSShaderAccumulator>                                           sorter;                 // 30
		BSCullingProcess*                                                        cullingProcess;         // 38
		BSTSmartPointer<BSWaterShaderMaterial, BSTSmartPointerIntrusiveRefCount> waterMaterial;          // 40
		float                                                                    reflectionUpdateTimer;  // 48
		std::uint32_t                                                            currentCubeMapFace;     // 4C
		CubeMapSide                                                              sortedCubeMapSide[6];   // 50
		bool                                                                     activeThisFrame;        // 80
	};
	static_assert(sizeof(TESWaterReflections) == 0x88);
}
