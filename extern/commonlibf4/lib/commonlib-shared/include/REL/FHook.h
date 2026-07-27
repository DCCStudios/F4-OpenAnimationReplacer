#pragma once

#include "REX/BASE.h"

#include "REL/IHook.h"

namespace REL
{
	class FHook :
		public IHook
	{
	public:
		FHook(const std::uintptr_t a_address);
		FHook(const std::uintptr_t a_address, const char* a_name);
		FHook(const std::uintptr_t a_address, const EHookType a_type);
		FHook(const std::uintptr_t a_address, const EHookType a_type, const EHookStep a_step);
		FHook(const std::uintptr_t a_address, const EHookStep a_step);
		FHook(const std::uintptr_t a_address, const char* a_name, const EHookStep a_step);
		FHook(const std::uintptr_t a_address, const char* a_name, const EHookType a_type);
		FHook(const std::uintptr_t a_address, const char* a_name, const EHookType a_type, const EHookStep a_step);

		~FHook();

		virtual bool        Init() override;
		virtual FHookHandle GetHandle() const override;
		virtual const char* GetName() const override;
		virtual EHookType   GetType() const override;
		virtual EHookStep   GetStep() const override;
		virtual std::size_t GetSize() const override;
		virtual std::size_t GetSizeTrampoline() const override;
		virtual bool        GetEnabled() const override;

	protected:
		std::uintptr_t m_address{ 0 };
		std::string    m_name;
		FHookHandle    m_handle;
		EHookType      m_type{ EHookType::None };
		EHookStep      m_step{ EHookStep::Load };
		std::size_t    m_size{ 8 };
		std::size_t    m_sizeTrampoline{ 0 };
		bool           m_enabled{ false };
	};
}
