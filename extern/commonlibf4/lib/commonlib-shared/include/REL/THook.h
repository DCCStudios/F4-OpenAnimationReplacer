#pragma once

#include "REX/BASE.h"

#include "REL/ASM.h"
#include "REL/FHook.h"
#include "REL/ID.h"
#include "REL/Offset.h"
#include "REL/Trampoline.h"
#include "REL/Utility.h"

#include "REX/LOG.h"

namespace REL
{
	template <class>
	class THook;

	template <class R, class... T>
	class THook<R(T...)> :
		public FHook
	{
	public:
		explicit THook(const ID a_id, const std::ptrdiff_t a_diff, R (*a_function)(T...)) :
			FHook(a_id.address() + a_diff)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			Detect();
		}

		explicit THook(const Offset a_offset, const std::ptrdiff_t a_diff, R (*a_function)(T...)) :
			FHook(a_offset.address() + a_diff)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			Detect();
		}

		explicit THook(const EHookStep a_step, const ID a_id, const std::ptrdiff_t a_diff, R (*a_function)(T...)) :
			FHook(a_id.address() + a_diff, a_step)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			Detect();
		}

		explicit THook(const EHookStep a_step, const Offset a_offset, const std::ptrdiff_t a_diff, R (*a_function)(T...)) :
			FHook(a_offset.address() + a_diff, a_step)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			Detect();
		}

		explicit THook(const char* a_name, const ID a_id, const std::ptrdiff_t a_diff, R (*a_function)(T...)) :
			FHook(a_id.address() + a_diff, a_name)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			Detect();
		}

		explicit THook(const char* a_name, const Offset a_offset, const std::ptrdiff_t a_diff, R (*a_function)(T...)) :
			FHook(a_offset.address() + a_diff, a_name)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			Detect();
		}

		explicit THook(const char* a_name, const EHookStep a_step, const ID a_id, const std::ptrdiff_t a_diff, R (*a_function)(T...)) :
			FHook(a_id.address() + a_diff, a_name, a_step)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			Detect();
		}

		explicit THook(const char* a_name, const EHookStep a_step, const Offset a_offset, const std::ptrdiff_t a_diff, R (*a_function)(T...)) :
			FHook(a_offset.address() + a_diff, a_name, a_step)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			Detect();
		}

		virtual bool Init() override
		{
			if (m_type == EHookType::None) {
				REX::ERROR("{}: could not determine hook type", *this);
				return false;
			}

			REL::Write(std::span{ m_bytesOld }, m_address);

			switch (m_type) {
				case EHookType::CALL5: {
					ASM::CALL5 assembly(m_address, GetTrampoline().allocate_branch5(m_function));
					REL::WriteData(std::span{ m_bytes }, assembly);
				} break;
				case EHookType::CALL6: {
					ASM::CALL6 assembly(m_address, GetTrampoline().allocate_branch6(m_function));
					REL::WriteData(std::span{ m_bytes }, assembly);
				} break;
				case EHookType::JMP5: {
					ASM::JMP5 assembly(m_address, GetTrampoline().allocate_branch5(m_function));
					REL::WriteData(std::span{ m_bytes }, assembly);
				} break;
				case EHookType::JMP6: {
					ASM::JMP6 assembly(m_address, GetTrampoline().allocate_branch6(m_function));
					REL::WriteData(std::span{ m_bytes }, assembly);
				} break;
			}

			REX::TRACE("{}: Init", *this);

			return true;
		}

		virtual bool Enable() override
		{
			if (m_type == EHookType::None) {
				REX::ERROR("{}: hook type is none", *this);
				return false;
			}

			REL::WriteSafe(m_address, std::span{ m_bytes });

			m_enabled = true;

			REX::TRACE("{}: Enabled", *this);

			return true;
		}

		virtual bool Disable() override
		{
			if (m_type == EHookType::None) {
				REX::ERROR("{}: hook type is none", *this);
				return false;
			}

			REL::WriteSafe(m_address, std::span{ m_bytesOld });

			m_enabled = false;

			REX::TRACE("{}: Disabled", *this);

			return false;
		}

		R operator()(T... a_args) const
		{
			assert(m_functionOld);
			return std::invoke(reinterpret_cast<std::decay_t<R(T...)>>(m_functionOld), a_args...);
		}

	private:
		void Detect()
		{
			const auto op = reinterpret_cast<std::uint8_t*>(m_address);
			switch (*op) {
				case 0xE8: {
					m_type = EHookType::CALL5;
					m_size = sizeof(ASM::CALL5);
					m_sizeTrampoline = sizeof(ASM::JMP14);
					m_functionOld = ASM::CALL5::TARGET(m_address);
				} break;
				case 0xE9: {
					m_type = EHookType::JMP5;
					m_size = sizeof(ASM::JMP5);
					m_sizeTrampoline = sizeof(ASM::JMP14);
					m_functionOld = ASM::JMP5::TARGET(m_address);
				} break;
				case 0xFF: {
					switch (*(op + 1)) {
						case 0x15: {
							m_type = EHookType::CALL6;
							m_size = sizeof(ASM::CALL6);
							m_sizeTrampoline = sizeof(std::uintptr_t);
							m_functionOld = ASM::CALL6::TARGET(m_address);
						} break;
						case 0x25: {
							m_type = EHookType::JMP6;
							m_size = sizeof(ASM::JMP6);
							m_sizeTrampoline = sizeof(std::uintptr_t);
							m_functionOld = ASM::JMP6::TARGET(m_address);
						} break;
					}
				} break;
			}

			m_bytes.resize(m_size);
			m_bytesOld.resize(m_size);
		}

	private:
		std::uintptr_t m_function;
		std::uintptr_t m_functionOld;

		std::vector<std::byte> m_bytes;
		std::vector<std::byte> m_bytesOld;
	};

	template <class R, class... T>
	THook(const ID, const std::uint64_t, R (*)(T...)) -> THook<R(T...)>;

	template <class R, class... T>
	THook(const Offset, const std::uint64_t, R (*)(T...)) -> THook<R(T...)>;

	template <class R, class... T>
	THook(const EHookStep, const ID, const std::uint64_t, R (*)(T...)) -> THook<R(T...)>;

	template <class R, class... T>
	THook(const EHookStep, const Offset, const std::uint64_t, R (*)(T...)) -> THook<R(T...)>;

	template <class R, class... T>
	THook(const char*, const ID, const std::uint64_t, R (*)(T...)) -> THook<R(T...)>;

	template <class R, class... T>
	THook(const char*, const Offset, const std::uint64_t, R (*)(T...)) -> THook<R(T...)>;

	template <class R, class... T>
	THook(const char*, const EHookStep, const ID, const std::uint64_t, R (*)(T...)) -> THook<R(T...)>;

	template <class R, class... T>
	THook(const char*, const EHookStep, const Offset, const std::uint64_t, R (*)(T...)) -> THook<R(T...)>;

	template <class T>
	using Hook [[deprecated("Renamed to 'REL::THook'")]] = THook<T>;
}

namespace REL
{
	template <class>
	class THookVFT;

	template <class R, class... T>
	class THookVFT<R(T...)> :
		public FHook
	{
	public:
		explicit THookVFT(const ID a_id, const std::size_t a_idx, R (*a_function)(T...)) :
			FHook(a_id.address() + (sizeof(void*) * a_idx), EHookType::VFT)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			m_functionOld = *reinterpret_cast<std::uintptr_t*>(m_address);
		}

		explicit THookVFT(const Offset a_offset, const std::size_t a_idx, R (*a_function)(T...)) :
			FHook(a_offset.address() + (sizeof(void*) * a_idx), EHookType::VFT)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			m_functionOld = *reinterpret_cast<std::uintptr_t*>(m_address);
		}

		explicit THookVFT(const EHookStep a_step, const ID a_id, const std::size_t a_idx, R (*a_function)(T...)) :
			FHook(a_id.address() + (sizeof(void*) * a_idx), EHookType::VFT, a_step)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			m_functionOld = *reinterpret_cast<std::uintptr_t*>(m_address);
		}

		explicit THookVFT(const EHookStep a_step, const Offset a_offset, const std::size_t a_idx, R (*a_function)(T...)) :
			FHook(a_offset.address() + (sizeof(void*) * a_idx), EHookType::VFT, a_step)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			m_functionOld = *reinterpret_cast<std::uintptr_t*>(m_address);
		}

		explicit THookVFT(const char* a_name, const ID a_id, const std::size_t a_idx, R (*a_function)(T...)) :
			FHook(a_id.address() + (sizeof(void*) * a_idx), a_name, EHookType::VFT)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			m_functionOld = *reinterpret_cast<std::uintptr_t*>(m_address);
		}

		explicit THookVFT(const char* a_name, const Offset a_offset, const std::size_t a_idx, R (*a_function)(T...)) :
			FHook(a_offset.address() + (sizeof(void*) * a_idx), a_name, EHookType::VFT)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			m_functionOld = *reinterpret_cast<std::uintptr_t*>(m_address);
		}

		explicit THookVFT(const char* a_name, const EHookStep a_step, const ID a_id, const std::size_t a_idx, R (*a_function)(T...)) :
			FHook(a_id.address() + (sizeof(void*) * a_idx), a_name, EHookType::VFT, a_step)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			m_functionOld = *reinterpret_cast<std::uintptr_t*>(m_address);
		}

		explicit THookVFT(const char* a_name, const EHookStep a_step, const Offset a_offset, const std::size_t a_idx, R (*a_function)(T...)) :
			FHook(a_offset.address() + (sizeof(void*) * a_idx), a_name, EHookType::VFT, a_step)
		{
			m_function = reinterpret_cast<std::uintptr_t>(a_function);
			m_functionOld = *reinterpret_cast<std::uintptr_t*>(m_address);
		}

		virtual bool Enable() override
		{
			if (!m_address) {
				REX::ERROR("{}: address is null", *this);
				return false;
			}

			REL::WriteSafeData(m_address, m_function);

			m_enabled = true;

			REX::TRACE("{}: Enabled", *this);

			return true;
		}

		virtual bool Disable() override
		{
			if (!m_address) {
				REX::ERROR("{}: address is null", *this);
				return false;
			}

			REL::WriteSafeData(m_address, m_functionOld);

			m_enabled = false;

			REX::TRACE("{}: Disabled", *this);

			return true;
		}

		R operator()(T... a_args) const
		{
			assert(m_functionOld);
			return std::invoke(reinterpret_cast<std::decay_t<R(T...)>>(m_functionOld), a_args...);
		}

	private:
		std::uintptr_t m_function;
		std::uintptr_t m_functionOld;
	};

	template <class R, class... T>
	THookVFT(const ID, const std::size_t, R (*)(T...)) -> THookVFT<R(T...)>;

	template <class R, class... T>
	THookVFT(const Offset, const std::size_t, R (*)(T...)) -> THookVFT<R(T...)>;

	template <class R, class... T>
	THookVFT(const EHookStep, const ID, const std::size_t, R (*)(T...)) -> THookVFT<R(T...)>;

	template <class R, class... T>
	THookVFT(const EHookStep, const Offset, const std::size_t, R (*)(T...)) -> THookVFT<R(T...)>;

	template <class R, class... T>
	THookVFT(const char*, const ID, const std::size_t, R (*)(T...)) -> THookVFT<R(T...)>;

	template <class R, class... T>
	THookVFT(const char*, const Offset, const std::size_t, R (*)(T...)) -> THookVFT<R(T...)>;

	template <class R, class... T>
	THookVFT(const char*, const EHookStep, const ID, const std::size_t, R (*)(T...)) -> THookVFT<R(T...)>;

	template <class R, class... T>
	THookVFT(const char*, const EHookStep, const Offset, const std::size_t, R (*)(T...)) -> THookVFT<R(T...)>;

	template <class T>
	using HookVFT [[deprecated("Renamed to 'REL::THookVFT'")]] = THookVFT<T>;
}
