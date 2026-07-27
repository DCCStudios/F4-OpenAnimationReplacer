#pragma once

#include "REL/IHook.h"
#include "REX/BASE.h"
#include "REX/TSingleton.h"

namespace REL
{
	class FHookStore :
		public REX::TSingleton<FHookStore>
	{
	public:
		FHookHandle Add(IHook* a_hook);
		void        Remove(const FHookHandle a_handle);

		void Init();

		void Enable();
		void Enable(const FHookHandle a_handle);
		void Enable(const EHookType a_type);
		void Enable(const EHookStep a_step);

		void Disable();
		void Disable(const FHookHandle a_handle);
		void Disable(const EHookType a_type);

		std::size_t GetSizeTrampoline();

	private:
		std::map<FHookHandle, IHook*> m_hooks;
		std::queue<FHookHandle>       m_hookQueue;
		std::uint32_t                 m_handleCount{ 0 };
	};
}
