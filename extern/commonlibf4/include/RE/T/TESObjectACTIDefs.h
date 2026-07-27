#pragma once

namespace RE
{
	namespace TESObjectACTIDefs
	{
		struct alignas(0x08) RadioReceiverData
		{
		public:
			BGSSoundOutput* outputOverride;  // 00
			float           frequency;       // 08
			float           volume;          // 0C
			bool            active;          // 10
			bool            noStatic;        // 11
		};
		static_assert(sizeof(RadioReceiverData) == 0x18);
	}
}
