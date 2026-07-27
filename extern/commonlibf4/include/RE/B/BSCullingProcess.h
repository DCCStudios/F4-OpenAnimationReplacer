#pragma once

#include "RE/N/NiAccumulator.h"
#include "RE/N/NiCullingProcess.h"

namespace RE
{
	class BSMultiBound;
	class BSOcclusionPlane;
	class BSPortalGraphEntry;

	class __declspec(novtable) alignas(0x08) BSCullingProcess :
		public NiCullingProcess  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::BSCullingProcess };
		static constexpr auto VTABLE{ VTABLE::BSCullingProcess };
		static constexpr auto Ni_RTTI{ Ni_RTTI::BSCullingProcess };

		enum class CullingType : std::uint32_t
		{
			kNormal = 0x0,
			kAllPass = 0x1,
			kAllFail = 0x2,
			kIgnoreMultiBounds = 0x3,
			kForceMultiBoundsNoUpdate = 0x4
		};

		~BSCullingProcess() override;  // 00

		// add
		virtual void AppendNonAccum(NiAVObject*);                    // 1C
		virtual void TestBaseVisibility(BSMultiBound* a_bound);      // 1D
		virtual void TestBaseVisibility(BSOcclusionPlane* a_plane);  // 1E
		virtual void TestBaseVisibility(const NiBound* a_bound);     // 1F

		// members
		BSTHashMap<NiAVObject*, bool> roomSharedMap;      // 120
		BSPortalGraphEntry*           portalGraphEntry;   // 150
		CullingType                   cullMode;           // 158
		CullingType                   typeStack[10];      // 15C
		std::uint32_t                 CTStackIndex;       // 184
		BSCompoundFrustum*            compoundFrustum;    // 188
		NiPointer<NiAccumulator>      accumulator;        // 190
		bool                          recurseToGeometry;  // 198
	};
	static_assert(sizeof(BSCullingProcess) == 0x1A0);
}
