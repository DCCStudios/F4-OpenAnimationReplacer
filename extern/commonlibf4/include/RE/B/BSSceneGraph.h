#pragma once

#include "RE//N/NiCamera.h"

namespace RE
{
	class __declspec(novtable) BSSceneGraph :
		public NiNode  // 000
	{
	public:
		static constexpr auto RTTI{ RTTI::BSSceneGraph };
		static constexpr auto VTABLE{ VTABLE::BSSceneGraph };
		static constexpr auto Ni_RTTI{ Ni_RTTI::BSSceneGraph };

		// add
		virtual float GetFarDistance();                               // 43
		virtual float GetNearDistance();                              // 44
		virtual void  SetViewDistanceBasedOnFrameRate(float a_time);  // 44

		// members
		NiPointer<NiCamera> camera;          // 140
		NiVisibleArray*     visArray;        // 148
		BSCullingProcess*   culler;          // 150
		bool                menuSceneGraph;  // 158
		float               currentFOV;      // 15C
	};
	static_assert(sizeof(BSSceneGraph) == 0x160);
}
