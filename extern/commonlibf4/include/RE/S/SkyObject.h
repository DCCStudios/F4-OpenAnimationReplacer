#pragma once

#include "RE/S/Sky.h"

namespace RE
{
	class __declspec(novtable) SkyObject
	{
	public:
		static constexpr auto RTTI{ RTTI::SkyObject };
		static constexpr auto VTABLE{ VTABLE::SkyObject };

		virtual ~SkyObject();

		// add
		virtual NiNode* GetRoot();                           // 01
		virtual void    Update(Sky* a_formal, float a_unk);  // 02

		// members
		NiPointer<NiNode> root;  // 08
	};
	static_assert(sizeof(SkyObject) == 0x10);
}
