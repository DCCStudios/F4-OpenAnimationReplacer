#pragma once

#include "REX/BASE.h"

namespace REX
{
	template <
		class E,
		class U = std::underlying_type_t<E>>
	class TEnum
	{
	public:
		using enum_type = E;
		using underlying_type = U;

		static_assert(std::is_enum_v<E>, "TEnum<E, ...> must be an enum");
		static_assert(std::is_integral_v<U>, "TEnum<..., U> must be an integral");

		constexpr TEnum() noexcept = default;
		constexpr TEnum(const TEnum&) noexcept = default;
		constexpr TEnum(TEnum&&) noexcept = default;

		template <class U2>  // NOLINTNEXTLINE(google-explicit-constructor)
		constexpr TEnum(TEnum<E, U2> a_rhs) noexcept :
			_impl(static_cast<U>(a_rhs.get()))
		{}

		constexpr TEnum(E a_value) noexcept :
			_impl(static_cast<U>(a_value))
		{}

		~TEnum() noexcept = default;

		constexpr TEnum& operator=(const TEnum&) noexcept = default;
		constexpr TEnum& operator=(TEnum&&) noexcept = default;

		template <class U2>
		constexpr TEnum& operator=(TEnum<E, U2> a_rhs) noexcept
		{
			_impl = static_cast<U>(a_rhs.get());
		}

		constexpr TEnum& operator=(E a_value) noexcept
		{
			_impl = static_cast<U>(a_value);
			return *this;
		}

	public:
		[[nodiscard]] explicit constexpr operator bool() const noexcept { return _impl != static_cast<U>(0); }

		[[nodiscard]] constexpr E operator*() const noexcept { return get(); }
		[[nodiscard]] constexpr E get() const noexcept { return static_cast<E>(_impl); }
		[[nodiscard]] constexpr U underlying() const noexcept { return _impl; }

	public:
		friend constexpr bool operator==(TEnum a_lhs, TEnum a_rhs) noexcept { return a_lhs.underlying() == a_rhs.underlying(); }
		friend constexpr bool operator==(TEnum a_lhs, E a_rhs) noexcept { return a_lhs.underlying() == static_cast<U>(a_rhs); }
		friend constexpr bool operator==(E a_lhs, TEnum a_rhs) noexcept { return static_cast<U>(a_lhs) == a_rhs.underlying(); }

	private:
		U _impl{ 0 };
	};

	template <class... Args>
	TEnum(Args...) -> TEnum<
					   std::common_type_t<Args...>,
					   std::underlying_type_t<
						   std::common_type_t<Args...>>>;
}

namespace REX
{
	template <
		class E,
		class U = std::underlying_type_t<E>>
	class [[deprecated("Renamed to 'REX::TEnum'")]] Enum :
		public TEnum<E, U>
	{
		using super = TEnum<E, U>;

	public:
		using enum_type = E;
		using underlying_type = U;

		using super::super;
		using super::operator=;
		using super::operator*;
	};

#pragma warning(push)
#pragma warning(disable: 4996)

	template <class... Args>
	Enum(Args...) -> Enum<
					  std::common_type_t<Args...>,
					  std::underlying_type_t<
						  std::common_type_t<Args...>>>;

#pragma warning(pop)
}
