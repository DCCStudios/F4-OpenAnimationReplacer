#pragma once

#include "RE/B/BSFixedString.h"

namespace RE
{
	class BGSLocation;

	class __declspec(novtable) LoadingMenuData :
		public IUIMessageData  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::LoadingMenuData };
		static constexpr auto VTABLE{ VTABLE::LoadingMenuData };

		// members
		BSFixedString action;    // 18
		BGSLocation*  location;  // 20
		bool          interior;  // 28
	};
	static_assert(sizeof(LoadingMenuData) == 0x30);
}
