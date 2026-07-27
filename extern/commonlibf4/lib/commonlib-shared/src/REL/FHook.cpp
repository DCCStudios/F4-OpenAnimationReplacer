#include "REL/FHook.h"

#include "REL/FHookStore.h"

#include "REX/LOG.h"

namespace REL
{
	FHook::FHook(const std::uintptr_t a_address) :
		m_address(a_address)
	{
		m_handle = FHookStore::GetSingleton()->Add(this);
		m_name = std::to_string(m_handle);
	}

	FHook::FHook(const std::uintptr_t a_address, const char* a_name) :
		m_address(a_address), m_name(a_name)
	{
		m_handle = FHookStore::GetSingleton()->Add(this);
	}

	FHook::FHook(const std::uintptr_t a_address, const EHookType a_type) :
		m_address(a_address), m_type(a_type)
	{
		m_handle = FHookStore::GetSingleton()->Add(this);
		m_name = std::to_string(m_handle);
	}

	FHook::FHook(const std::uintptr_t a_address, const EHookType a_type, const EHookStep a_step) :
		m_address(a_address), m_type(a_type), m_step(a_step)
	{
		m_handle = FHookStore::GetSingleton()->Add(this);
		m_name = std::to_string(m_handle);
	}

	FHook::FHook(const std::uintptr_t a_address, const EHookStep a_step) :
		m_address(a_address), m_step(a_step)
	{
		m_handle = FHookStore::GetSingleton()->Add(this);
		m_name = std::to_string(m_handle);
	}

	FHook::FHook(const std::uintptr_t a_address, const char* a_name, const EHookStep a_step) :
		m_address(a_address), m_name(a_name), m_step(a_step)
	{
		m_handle = FHookStore::GetSingleton()->Add(this);
	}

	FHook::FHook(const std::uintptr_t a_address, const char* a_name, const EHookType a_type) :
		m_address(a_address), m_name(a_name), m_type(a_type)
	{
		m_handle = FHookStore::GetSingleton()->Add(this);
	}

	FHook::FHook(const std::uintptr_t a_address, const char* a_name, const EHookType a_type, const EHookStep a_step) :
		m_address(a_address), m_name(a_name), m_type(a_type), m_step(a_step)
	{
		m_handle = FHookStore::GetSingleton()->Add(this);
	}

	FHook::~FHook()
	{
		FHookStore::GetSingleton()->Remove(m_handle);
	}

	bool FHook::Init()
	{
		REX::TRACE("{}: Init", *this);

		return true;
	}

	FHookHandle FHook::GetHandle() const
	{
		return m_handle;
	}

	const char* FHook::GetName() const
	{
		return m_name.c_str();
	}

	EHookType FHook::GetType() const
	{
		return m_type;
	}

	EHookStep FHook::GetStep() const
	{
		return m_step;
	}

	std::size_t FHook::GetSize() const
	{
		return m_size;
	}

	std::size_t FHook::GetSizeTrampoline() const
	{
		return m_sizeTrampoline;
	}

	bool FHook::GetEnabled() const
	{
		return m_enabled;
	}
}
