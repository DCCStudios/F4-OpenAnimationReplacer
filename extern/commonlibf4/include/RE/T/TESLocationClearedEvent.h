#pragma once

namespace RE
{
	class TESLocationClearedEvent
	{
	public:
		[[nodiscard]] static BSTEventSource<TESLocationClearedEvent>* GetEventSource()
		{
			using func_t = decltype(&TESLocationClearedEvent::GetEventSource);
			static REL::Relocation<func_t> func{ ID::TESLocationClearedEvent::GetEventSource };
			return func();
		}

		// members
		const BGSLocation* location;  // 00
	};
	static_assert(sizeof(TESLocationClearedEvent) == 0x08);
}
