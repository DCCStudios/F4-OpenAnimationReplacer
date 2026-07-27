#pragma once

#include "RE/H/hkArray.h"
#include "RE/H/hkBool.h"

namespace RE
{
	class hkBlockStreamAllocator;

	class hkBlockStreamBase
	{
	public:
		class Stream;

		class Block
		{
		public:
			// members
			std::uint32_t           m_numElementsAndBytesUsed;  // 0x00
			std::int32_t            m_blockIndexInStream;       // 0x04
			Block*                  m_next;                     // 0x08
			hkBlockStreamAllocator* m_allocator;                // 0x10
			Stream*                 m_blockStream;              // 0x18
			std::byte               m_data[4064];               // 0x20
		};
		static_assert(sizeof(Block) == 0x1000);

		class Stream
		{
		public:
			// members
			hkBlockStreamAllocator*             m_allocator;                              // 0x00
			std::int32_t                        m_numTotalElements;                       // 0x08
			Stream*                             m_blockStreamPPU;                         // 0x10
			hkBool                              m_partiallyFreed;                         // 0x18
			hkBool                              m_zeroNewBlocks;                          // 0x19
			hkBool                              m_isLocked;                               // 0x1A
			hkBool                              m_spuWronglySentConsumedBlockStreamBack;  // 0x1B
			hkInplaceArrayAligned16<Block*, 24> m_blocks;                                 // 0x20
		};
		static_assert(sizeof(Stream) == 0x100);
	};
	static_assert(std::is_empty_v<hkBlockStreamBase>);
}
