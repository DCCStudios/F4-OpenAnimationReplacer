#pragma once

#include "REX/FModule.h"

namespace REL
{
	class Offset
	{
		friend class VariantID;
	public:
		static_assert(COMMONLIB_RUNTIMECOUNT > 0, "COMMONLIB_RUNTIMECOUNT must be at least 1.");

		constexpr Offset() noexcept = default;

		explicit constexpr Offset(std::size_t a_offset) noexcept
		{
			for (auto& offset : m_offsets)
			{
				offset = a_offset;
			}
		}

		explicit constexpr Offset(std::initializer_list<std::size_t> a_list) noexcept
		{
			if (a_list.size() == 0)
			{
				return;
			}

			std::size_t i = 0;
			std::size_t lastValue = 0;

			for (auto val : a_list)
			{
				if (i >= COMMONLIB_RUNTIMECOUNT)
				{
					break;
				}

				m_offsets[i++] = val;
				lastValue = val;
			}

			while (i < COMMONLIB_RUNTIMECOUNT)
			{
				m_offsets[i++] = lastValue;
			}
		}

		constexpr Offset& operator=(std::size_t a_offset) noexcept
		{
			for (auto& offset : m_offsets)
			{
				offset = a_offset;
			}
			return *this;
		}

		[[nodiscard]] std::uintptr_t address() const
		{
			const auto mod = REX::FModule::GetExecutingModule();
			return mod.GetBaseAddress() + offset();
		}

		[[nodiscard]] std::size_t offset() const noexcept
		{
			auto index = static_cast<std::uint8_t>(REX::FModule::GetRuntimeIndex());

			if (index >= COMMONLIB_RUNTIMECOUNT)
				index = COMMONLIB_RUNTIMECOUNT - 1;

			return m_offsets[index];
		}

	private:
		std::size_t m_offsets[COMMONLIB_RUNTIMECOUNT]{ 0 };
	};
}
