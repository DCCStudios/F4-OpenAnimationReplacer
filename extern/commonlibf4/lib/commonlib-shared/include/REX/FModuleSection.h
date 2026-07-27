#pragma once

#include "REX/BASE.h"

namespace REX
{
	class FModuleSection
	{
	public:
		constexpr FModuleSection() noexcept = default;

		constexpr FModuleSection(std::uintptr_t a_base, std::uintptr_t a_address, std::uintptr_t a_size) noexcept :
			m_base(a_base),
			m_address(a_address),
			m_size(a_size)
		{}

		[[nodiscard]] constexpr std::uintptr_t GetAddress() const noexcept
		{
			return m_address;
		}

		[[nodiscard]] constexpr std::ptrdiff_t GetOffset() const noexcept
		{
			return GetAddress() - m_base;
		}

		[[nodiscard]] constexpr std::size_t GetSize() const noexcept
		{
			return m_size;
		}

		template <class T = void>
		[[nodiscard]] T* GetPointer() const noexcept
		{
			return reinterpret_cast<T*>(GetAddress());
		}

	private:
		std::uintptr_t m_base{ 0 };
		std::uintptr_t m_address{ 0 };
		std::size_t    m_size{ 0 };
	};
}
