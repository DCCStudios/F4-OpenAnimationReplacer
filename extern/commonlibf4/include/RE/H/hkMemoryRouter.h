#pragma once

#include "RE/H/hkLifoAllocator.h"

namespace RE
{
	class hkMemoryAllocator;

	class hkMemoryRouter
	{
	public:
		using Allocator = hkMemoryAllocator;
		using Stack = hkLifoAllocator;

		[[nodiscard]] static hkMemoryRouter& GetInstance()
		{
			return *GetInstancePtr();
		}

		[[nodiscard]] static hkMemoryRouter* GetInstancePtr()
		{
			static REL::Relocation<std::uint32_t*> slotID{ ID::hkMemoryRouter::tlsSlotID };
			return static_cast<hkMemoryRouter*>(REX::W32::TlsGetValue(*slotID));
		}

		static void ReplaceInstance(hkMemoryRouter* a_router)
		{
			static REL::Relocation<std::uint32_t*> slotID{ ID::hkMemoryRouter::tlsSlotID };
			REX::W32::TlsSetValue(*slotID, a_router);
		}

		void                 SetDebug(Allocator* a_val) noexcept { debug = a_val; }
		void                 SetHeap(Allocator* a_val) noexcept { heap = a_val; }
		void                 SetSolver(Allocator* a_val) noexcept { solver = a_val; }
		void                 SetTemp(Allocator* a_val) noexcept { temp = a_val; }
		[[nodiscard]] Stack& GetStack() noexcept { return stack; }

		// members
		Stack              stack;                  // 0x00
		hkMemoryAllocator* temp{ nullptr };        // 0x50
		hkMemoryAllocator* heap{ nullptr };        // 0x58
		hkMemoryAllocator* debug{ nullptr };       // 0x60
		hkMemoryAllocator* solver{ nullptr };      // 0x68
		void*              userData{ nullptr };    // 0x70
		std::uint32_t      refObjLocalStore{ 0 };  // 0x78
	};
	static_assert(sizeof(hkMemoryRouter) == 0x80);
}
