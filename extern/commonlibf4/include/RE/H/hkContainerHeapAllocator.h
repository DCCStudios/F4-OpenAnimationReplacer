#pragma once

#include "RE/H/hkMemoryAllocator.h"

namespace RE
{
	class hkContainerHeapAllocator
	{
	public:
		class __declspec(novtable) Allocator :
			public hkMemoryAllocator
		{
		public:
			static constexpr auto RTTI{ RTTI::hkContainerHeapAllocator__Allocator };
			static constexpr auto VTABLE{ VTABLE::hkContainerHeapAllocator__Allocator };

			~Allocator() override;

			// override (hkMemoryAllocator)
			virtual void*        BlockAlloc(std::int32_t a_numBytes) override;                                 // 01
			virtual void         BlockFree(void* a_ptr, std::int32_t a_numBytes) override;                     // 02
			virtual void*        BufAlloc(std::int32_t& a_reqNumBytesInOut) override;                          // 03
			virtual void         BufFree(void* a_ptr, std::int32_t a_numBytes) override;                       // 04
			virtual void         GetMemoryStatistics(MemoryStatistics& a_usage) const override;                // 08
			virtual std::int32_t GetAllocatedSize(const void* a_obj, std::int32_t a_numBytes) const override;  // 09
		};
		static_assert(sizeof(Allocator) == 0x08);

		[[nodiscard]] static Allocator* GetSingleton()
		{
			static REL::Relocation<hkContainerHeapAllocator::Allocator*> singleton{ ID::hkContainerHeapAllocator::Singleton };
			return singleton.get();
		}
	};
	static_assert(std::is_empty_v<hkContainerHeapAllocator>);
}
