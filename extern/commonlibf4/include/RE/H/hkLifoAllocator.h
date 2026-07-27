#pragma once

#include "RE/H/hkMemoryAllocator.h"

namespace RE
{
	class __declspec(novtable) hkLifoAllocator :
		public hkMemoryAllocator  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::hkLifoAllocator };
		static constexpr auto VTABLE{ VTABLE::hkLifoAllocator };

		class Implementation;

		hkLifoAllocator() { REX::EMPLACE_VTABLE(this); }

		// override (hkMemoryAllocator)
		void*        BlockAlloc(std::int32_t a_numBytesIn) override;                                                  // 01
		void         BlockFree(void* a_ptr, std::int32_t a_numBytesIn) override;                                      // 02
		void*        BufAlloc(std::int32_t& a_reqNumBytesInOut) override;                                             // 03
		void         BufFree(void* a_ptr, std::int32_t a_numBytes) override;                                          // 04
		void*        BufRealloc(void* a_old, std::int32_t a_oldNumBytes, std::int32_t& a_reqNumBytesInOut) override;  // 05
		void         GetMemoryStatistics(MemoryStatistics& a_stats) const override;                                   // 08
		std::int32_t GetAllocatedSize(const void* a_obj, std::int32_t a_numBytes) const override;                     // 09

		void Init(hkMemoryAllocator* a_slabAllocator, hkMemoryAllocator* a_largeAllocator, hkMemoryAllocator* a_internalAllocator)
		{
			using func_t = decltype(&hkLifoAllocator::Init);
			static REL::Relocation<func_t> func{ REL::ID(1417289) };
			return func(this, a_slabAllocator, a_largeAllocator, a_internalAllocator);
		}

		void Quit(hkMemoryAllocator** a_allocators)
		{
			using func_t = decltype(&hkLifoAllocator::Quit);
			static REL::Relocation<func_t> func{ REL::ID(1058127) };
			return func(this, a_allocators);
		}

		// members
		Implementation*    m_impl{ nullptr };               // 0x08
		const std::int32_t m_slabSize{ 0x8000 };            // 0x10
		void*              m_cur{ nullptr };                // 0x18
		void*              m_end{ nullptr };                // 0x20
		void*              m_firstNonLifoEnd{ nullptr };    // 0x28
		void*              m_cachedEmptySlab{ nullptr };    // 0x30
		hkMemoryAllocator* m_slabAllocator{ nullptr };      // 0x38
		hkMemoryAllocator* m_largeAllocator{ nullptr };     // 0x40
		hkMemoryAllocator* m_internalAllocator{ nullptr };  // 0x48
	};
	static_assert(sizeof(hkLifoAllocator) == 0x50);
}
