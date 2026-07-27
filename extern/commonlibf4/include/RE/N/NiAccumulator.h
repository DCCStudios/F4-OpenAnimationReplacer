#pragma once

#include "RE/N/NiVisibleArray.h"

namespace RE
{
	class __declspec(novtable) NiAccumulator :
		public NiObject  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::NiAccumulator };
		static constexpr auto VTABLE{ VTABLE::NiAccumulator };
		static constexpr auto Ni_RTTI{ Ni_RTTI::NiAccumulator };

		// add
		virtual void StartAccumulating(const NiCamera* a_camera);              // 28
		virtual void FinishAccumulating();                                     // 29
		virtual void RegisterObjectArray(NiVisibleArray* a_unk0);              // 2A
		virtual void StartGroupingAlphas(const NiBound* a_unk0, bool a_unk1);  // 2B
		virtual void StopGroupingAlphas();                                     // 2C
		virtual void RegisterObject(BSGeometry* a_unk0);                       // 2D

		// members
		const NiCamera* camera;  // 10
	};
	static_assert(sizeof(NiAccumulator) == 0x18);
}
