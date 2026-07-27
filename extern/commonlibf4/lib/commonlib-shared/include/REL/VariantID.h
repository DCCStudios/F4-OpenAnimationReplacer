#pragma once

#include "REL/ID.h"
#include "REL/Offset.h"

namespace REL
{
	class VariantID
	{
		static_assert(COMMONLIB_RUNTIMECOUNT > 0, "COMMONLIB_RUNTIMECOUNT must be at least 1.");

		struct Variant
		{
			enum class Method : std::uint8_t
			{
				kOffset = 0,
				kID
			};

			Method        method{ Method::kID };
			std::uint64_t value{ 0 };

			constexpr Variant& operator=(const Variant& a_variant) noexcept
			{
				method = a_variant.method;
				value = a_variant.value;

				return *this;
			}
		};
	public:
		constexpr VariantID() noexcept = default;

		explicit constexpr VariantID(std::uint64_t a_id) noexcept
		{
			for (auto& off : m_offs)
				off = { Variant::Method::kID, a_id };
		}

		template <typename... Args>
		explicit constexpr VariantID(Args&&... args) noexcept
		{
			auto size = sizeof...(args);
			if (!size)
				return;

			std::size_t	i = 0;
			Variant     lastValue;

			inserter(i, lastValue, std::forward<Args>(args)...);

			while (i < COMMONLIB_RUNTIMECOUNT)
				m_offs[i++] = lastValue;
		}

		constexpr VariantID& operator=(std::uint64_t a_id) noexcept
		{
			for (auto& off : m_offs)
				off = { Variant::Method::kID, a_id };

			return *this;
		}

		[[nodiscard]] std::uintptr_t address() const
		{
			const auto mod = REX::FModule::GetExecutingModule();
			return mod.GetBaseAddress() + offset();
		}

		[[nodiscard]] std::size_t offset() const
		{
			auto index = static_cast<std::uint8_t>(REX::FModule::GetRuntimeIndex());

			if (index >= COMMONLIB_RUNTIMECOUNT)
				index = COMMONLIB_RUNTIMECOUNT - 1;

			if (m_offs[index].method == Variant::Method::kOffset)
				return m_offs[index].value;

			const auto iddb = IDDB::GetSingleton();
			return iddb->offset(m_offs[index].value);
		}
	private:
		template <typename T>
		constexpr void inserter(std::size_t& i, Variant& last, T item) noexcept
		{
			if (i >= COMMONLIB_RUNTIMECOUNT)
				return;

			if constexpr (std::is_same_v<T, REL::ID>)
			{
				m_offs[i] = { Variant::Method::kID, item.m_ids[0] };
				last = m_offs[i++];
			} else if constexpr (std::is_same_v<T, REL::Offset>)
			{
				m_offs[i] = { Variant::Method::kOffset, item.m_offsets[0] };
				last = m_offs[i++];
			} else if (std::is_integral_v<T>)
			{
				m_offs[i] = { Variant::Method::kID, (const std::uint64_t)item };
				last = m_offs[i++];
			}
		}

		template <typename T, typename... Args>
		constexpr void inserter(std::size_t& i, Variant& last, T item, Args... args) noexcept
		{
			inserter(i, last, item);
			inserter(i, last, args...);
		}

		Variant m_offs[COMMONLIB_RUNTIMECOUNT]{};
	};
}
