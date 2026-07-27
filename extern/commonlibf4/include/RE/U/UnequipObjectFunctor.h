#pragma once

namespace RE
{
	class __declspec(novtable) UnequipObjectFunctor
	{
	public:
		// members
		Actor* actor;  // 00
	};
	static_assert(sizeof(UnequipObjectFunctor) == 0x08);
}
