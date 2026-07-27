#include "REL/FHookStore.h"

#include "REL/IHook.h"

#include "REX/LOG.h"

namespace REL
{
	FHookHandle FHookStore::Add(IHook* a_hook)
	{
		if (a_hook && a_hook->GetHandle() == 0) {
			m_hooks.insert({ ++m_handleCount, a_hook });
			m_hookQueue.push(m_handleCount);
			return m_handleCount;
		}

		return 0;
	}

	void FHookStore::Remove(const FHookHandle a_handle)
	{
		m_hooks.erase(a_handle);
	}

	void FHookStore::Init()
	{
		std::size_t count{ 0 };
		while (!m_hookQueue.empty()) {
			if (m_hooks[m_hookQueue.front()]->Init())
				count++;

			m_hookQueue.pop();
		}

		REX::DEBUG("FHookStore: Init {} queued hooks", count);
	}

	void FHookStore::Enable()
	{
		std::size_t count{ 0 };
		for (auto& [name, hook] : m_hooks) {
			if (hook) {
				if (hook->Enable())
					count++;
			}
		}

		REX::DEBUG("FHookStore: Enabled {} hooks", count);
	}

	void FHookStore::Enable(const FHookHandle a_handle)
	{
		if (const auto it = m_hooks.find(a_handle); it != m_hooks.end()) {
			if (it->second) {
				it->second->Enable();
			}
		}
	}

	void FHookStore::Enable(const EHookType a_type)
	{
		for (auto& [name, hook] : m_hooks) {
			if (hook && hook->GetType() == a_type) {
				hook->Enable();
			}
		}
	}

	void FHookStore::Enable(const EHookStep a_step)
	{
		std::size_t count{ 0 };
		for (auto& [name, hook] : m_hooks) {
			if (hook && hook->GetStep() == a_step) {
				if (hook->Enable())
					count++;
			}
		}

		REX::DEBUG("FHookStore: Enabled {} {} hooks", count, a_step);
	}

	void FHookStore::Disable()
	{
		std::size_t count{ 0 };
		for (auto& [name, hook] : m_hooks) {
			if (hook) {
				if (hook->Disable())
					count++;
			}
		}

		REX::DEBUG("FHookStore: Disabled {} hooks", count);
	}

	void FHookStore::Disable(const FHookHandle a_handle)
	{
		for (auto& [name, hook] : m_hooks) {
			if (hook && hook->GetHandle() == a_handle) {
				hook->Disable();
			}
		}
	}

	void FHookStore::Disable(const EHookType a_type)
	{
		for (auto& [name, hook] : m_hooks) {
			if (hook && hook->GetType() == a_type) {
				hook->Disable();
			}
		}
	}

	std::size_t FHookStore::GetSizeTrampoline()
	{
		std::size_t size{ 0 };
		for (auto& [name, hook] : m_hooks) {
			if (hook) {
				size += hook->GetSizeTrampoline();
			}
		}

		return size;
	}
}
