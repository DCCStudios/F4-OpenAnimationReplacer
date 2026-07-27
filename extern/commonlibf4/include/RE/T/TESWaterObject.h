#pragma once

namespace RE
{
	class BSMultiBoundAABB;
	class TESWaterNormals;
	class TESWaterDisplacement;
	class TESWaterReflections;

	class __declspec(novtable) TESWaterObject :
		public NiRefObject  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::TESWaterObject };
		static constexpr auto VTABLE{ VTABLE::TESWaterObject };

		enum class Flags
		{
			kVisible = 0x0,
			kLOD = 0x2,
			kNoDisplacements = 0x4,
			kProceduralWater = 0x8
		};

		// members
		NiPlane                                                      waterPlane;          // 10
		NiPointer<NiAVObject>                                        waterObject;         // 20
		NiPointer<NiAVObject>                                        waterRipplesObject;  // 28
		TESWaterForm*                                                waterType;           // 30
		NiPointer<TESWaterReflections>                               waterReflections;    // 38
		NiPointer<TESWaterNormals>                                   waterNormals;        // 40
		NiPointer<TESWaterDisplacement>                              waterDisplacement;   // 48
		BSTArray<NiPointer<BSMultiBoundAABB>, BSTArrayHeapAllocator> multiBoundShape;     // 50
		std::uint8_t                                                 flags;               // 68
	};
	static_assert(sizeof(TESWaterObject) == 0x70);
}
