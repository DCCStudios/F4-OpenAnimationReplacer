#pragma once

namespace RE
{
	namespace BSCoreUtils
	{
		struct BSCPUFeatures
		{
			bool x64;       // 00
			bool FPU;       // 01
			bool MMX;       // 02
			bool MMXEXT;    // 03
			bool SSE;       // 04
			bool SSE2;      // 05
			bool SSE3;      // 06
			bool SSSE3;     // 07
			bool SSSE41;    // 08
			bool SSE42;     // 09
			bool SSE4a;     // 0A
			bool AVX;       // 0B
			bool AVX2;      // 0C
			bool AVX512;    // 0D
			bool AVX512DQ;  // 0E
			bool AVX512PF;  // 0F
			bool AVX512ER;  // 10
			bool AVX512CD;  // 11
			bool AVX512BW;  // 12
			bool AVX512VL;  // 13
			bool AES;       // 14
			bool ADX;       // 15
			bool SHA;       // 16
			bool XOP;       // 17
			bool FMA3;      // 18
			bool FMA4;      // 19
		};
		static_assert(sizeof(BSCPUFeatures) == 0x1A);

		struct StackTrace
		{
			std::uint32_t numFrames;   // 00
			std::int64_t  frames[64];  // 08
		};
		static_assert(sizeof(StackTrace) == 0x208);
	}
}
