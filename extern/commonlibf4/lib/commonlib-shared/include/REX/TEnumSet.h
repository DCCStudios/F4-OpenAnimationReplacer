#pragma once

#include "REX/BASE.h"

namespace REX
{
	template <
		class E,
		class U = std::underlying_type_t<E>>
	class TEnumSet
	{
	public:
		using enum_type = E;
		using underlying_type = U;

		static_assert(std::is_enum_v<E>, "TEnumSet<E, ...> must be an enum");
		static_assert(std::is_integral_v<U>, "TEnumSet<..., U> must be an integral");

		constexpr TEnumSet() noexcept = default;
		constexpr TEnumSet(const TEnumSet&) noexcept = default;
		constexpr TEnumSet(TEnumSet&&) noexcept = default;

		template <class U2>  // NOLINTNEXTLINE(google-explicit-constructor)
		constexpr TEnumSet(TEnumSet<E, U2> a_rhs) noexcept :
			_impl(static_cast<U>(a_rhs.get()))
		{}

		template <class... Args>
		constexpr TEnumSet(Args... a_values) noexcept
			requires(std::same_as<Args, E> && ...)
			:
			_impl((static_cast<U>(a_values) | ...))
		{}

		~TEnumSet() noexcept = default;

		constexpr TEnumSet& operator=(const TEnumSet&) noexcept = default;
		constexpr TEnumSet& operator=(TEnumSet&&) noexcept = default;

		template <class U2>
		constexpr TEnumSet& operator=(TEnumSet<E, U2> a_rhs) noexcept
		{
			_impl = static_cast<U>(a_rhs.get());
			return *this;
		}

		constexpr TEnumSet& operator=(E a_value) noexcept
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
		template <class... Args>
		constexpr TEnumSet& set(Args... a_args) noexcept
			requires(std::same_as<Args, E> && ...)
		{
			_impl |= (static_cast<U>(a_args) | ...);
			return *this;
		}

		template <class... Args>
		constexpr TEnumSet& set(bool a_set, Args... a_args) noexcept
			requires(std::same_as<Args, E> && ...)
		{
			if (a_set)
				_impl |= (static_cast<U>(a_args) | ...);
			else
				_impl &= ~(static_cast<U>(a_args) | ...);

			return *this;
		}

		template <class... Args>
		constexpr TEnumSet& reset(Args... a_args) noexcept
			requires(std::same_as<Args, E> && ...)
		{
			_impl &= ~(static_cast<U>(a_args) | ...);
			return *this;
		}

		constexpr TEnumSet& reset() noexcept
		{
			_impl = 0;
			return *this;
		}

		template <class... Args>
		[[nodiscard]] constexpr bool any(Args... a_args) const noexcept
			requires(std::same_as<Args, E> && ...)
		{
			return (_impl & (static_cast<U>(a_args) | ...)) != static_cast<U>(0);
		}

		template <class... Args>
		[[nodiscard]] constexpr bool all(Args... a_args) const noexcept
			requires(std::same_as<Args, E> && ...)
		{
			return (_impl & (static_cast<U>(a_args) | ...)) == (static_cast<U>(a_args) | ...);
		}

		template <class... Args>
		[[nodiscard]] constexpr bool none(Args... a_args) const noexcept
			requires(std::same_as<Args, E> && ...)
		{
			return (_impl & (static_cast<U>(a_args) | ...)) == static_cast<U>(0);
		}

	public:
		friend constexpr bool operator==(TEnumSet a_lhs, TEnumSet a_rhs) noexcept { return a_lhs.underlying() == a_rhs.underlying(); }
		friend constexpr bool operator==(TEnumSet a_lhs, E a_rhs) noexcept { return a_lhs.underlying() == static_cast<U>(a_rhs); }
		friend constexpr bool operator==(E a_lhs, TEnumSet a_rhs) noexcept { return static_cast<U>(a_lhs) == a_rhs.underlying(); }

		friend constexpr std::strong_ordering operator<=>(TEnumSet a_lhs, TEnumSet a_rhs) noexcept { return a_lhs.underlying() <=> a_rhs.underlying(); }
		friend constexpr std::strong_ordering operator<=>(TEnumSet a_lhs, E a_rhs) noexcept { return a_lhs.underlying() <=> static_cast<U>(a_rhs); }
		friend constexpr std::strong_ordering operator<=>(E a_lhs, TEnumSet a_rhs) noexcept { return static_cast<U>(a_lhs) <=> a_rhs.underlying(); }

		friend constexpr TEnumSet operator&(TEnumSet a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() & a_rhs.underlying()); }
		friend constexpr TEnumSet operator&(TEnumSet a_lhs, E a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() & static_cast<U>(a_rhs)); }
		friend constexpr TEnumSet operator&(E a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(static_cast<U>(a_lhs) & a_rhs.underlying()); }

		friend constexpr TEnumSet& operator&=(TEnumSet& a_lhs, TEnumSet a_rhs) noexcept { return a_lhs = a_lhs & a_rhs; }
		friend constexpr TEnumSet& operator&=(TEnumSet& a_lhs, E a_rhs) noexcept { return a_lhs = a_lhs & a_rhs; }

		friend constexpr TEnumSet operator|(TEnumSet a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() | a_rhs.underlying()); }
		friend constexpr TEnumSet operator|(TEnumSet a_lhs, E a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() | static_cast<U>(a_rhs)); }
		friend constexpr TEnumSet operator|(E a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(static_cast<U>(a_lhs) | a_rhs.underlying()); }

		friend constexpr TEnumSet& operator|=(TEnumSet& a_lhs, TEnumSet a_rhs) noexcept { return a_lhs = a_lhs | a_rhs; }
		friend constexpr TEnumSet& operator|=(TEnumSet& a_lhs, E a_rhs) noexcept { return a_lhs = a_lhs | a_rhs; }

		friend constexpr TEnumSet operator^(TEnumSet a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() ^ a_rhs.underlying()); }
		friend constexpr TEnumSet operator^(TEnumSet a_lhs, E a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() ^ static_cast<U>(a_rhs)); }
		friend constexpr TEnumSet operator^(E a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(static_cast<U>(a_lhs) ^ a_rhs.underlying()); }

		friend constexpr TEnumSet& operator^=(TEnumSet& a_lhs, TEnumSet a_rhs) noexcept { return a_lhs = a_lhs ^ a_rhs; }
		friend constexpr TEnumSet& operator^=(TEnumSet& a_lhs, E a_rhs) noexcept { return a_lhs = a_lhs ^ a_rhs; }

		friend constexpr TEnumSet operator+(TEnumSet a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() + a_rhs.underlying()); }
		friend constexpr TEnumSet operator+(TEnumSet a_lhs, E a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() + static_cast<U>(a_rhs)); }
		friend constexpr TEnumSet operator+(E a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(static_cast<U>(a_lhs) + a_rhs.underlying()); }

		friend constexpr TEnumSet& operator+=(TEnumSet& a_lhs, TEnumSet a_rhs) noexcept { return a_lhs = a_lhs + a_rhs; }
		friend constexpr TEnumSet& operator+=(TEnumSet& a_lhs, E a_rhs) noexcept { return a_lhs = a_lhs + a_rhs; }

		friend constexpr TEnumSet operator-(TEnumSet a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() - a_rhs.underlying()); }
		friend constexpr TEnumSet operator-(TEnumSet a_lhs, E a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() - static_cast<U>(a_rhs)); }
		friend constexpr TEnumSet operator-(E a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(static_cast<U>(a_lhs) - a_rhs.underlying()); }

		friend constexpr TEnumSet& operator-=(TEnumSet& a_lhs, TEnumSet a_rhs) noexcept { return a_lhs = a_lhs - a_rhs; }
		friend constexpr TEnumSet& operator-=(TEnumSet& a_lhs, E a_rhs) noexcept { return a_lhs = a_lhs - a_rhs; }

		friend constexpr TEnumSet operator<<(TEnumSet a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() << a_rhs.underlying()); }
		friend constexpr TEnumSet operator<<(TEnumSet a_lhs, E a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() << static_cast<U>(a_rhs)); }
		friend constexpr TEnumSet operator<<(E a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(static_cast<U>(a_lhs) << a_rhs.underlying()); }

		friend constexpr TEnumSet& operator<<=(TEnumSet& a_lhs, TEnumSet a_rhs) noexcept { return a_lhs = a_lhs << a_rhs; }
		friend constexpr TEnumSet& operator<<=(TEnumSet& a_lhs, E a_rhs) noexcept { return a_lhs = a_lhs << a_rhs; }

		friend constexpr TEnumSet operator>>(TEnumSet a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() >> a_rhs.underlying()); }
		friend constexpr TEnumSet operator>>(TEnumSet a_lhs, E a_rhs) noexcept { return static_cast<E>(a_lhs.underlying() >> static_cast<U>(a_rhs)); }
		friend constexpr TEnumSet operator>>(E a_lhs, TEnumSet a_rhs) noexcept { return static_cast<E>(static_cast<U>(a_lhs) >> a_rhs.underlying()); }

		friend constexpr TEnumSet& operator>>=(TEnumSet& a_lhs, TEnumSet a_rhs) noexcept { return a_lhs = a_lhs >> a_rhs; }
		friend constexpr TEnumSet& operator>>=(TEnumSet& a_lhs, E a_rhs) noexcept { return a_lhs = a_lhs >> a_rhs; }

		friend constexpr TEnumSet& operator~(TEnumSet& a_lhs) noexcept { return a_lhs = ~a_lhs.underlying(); }

	private:
		U _impl{ 0 };
	};

	template <class... Args>
	TEnumSet(Args...) -> TEnumSet<
						  std::common_type_t<Args...>,
						  std::underlying_type_t<
							  std::common_type_t<Args...>>>;
}

namespace REX
{
	template <
		class E,
		class U = std::underlying_type_t<E>>
	class [[deprecated("Renamed to 'REX::TEnumSet'")]] EnumSet :
		public TEnumSet<E, U>
	{
		using super = TEnumSet<E, U>;

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
	EnumSet(Args...) -> EnumSet<
						 std::common_type_t<Args...>,
						 std::underlying_type_t<
							 std::common_type_t<Args...>>>;

#pragma warning(pop)
}

#define REX_DEFINE_ENUM_CLASS_FLAGS(E)                                                                                                     \
	inline constexpr E&   operator|=(E& a_lhs, E a_rhs) { return a_lhs = (E)((__underlying_type(E))a_lhs | (__underlying_type(E))a_rhs); } \
	inline constexpr E&   operator&=(E& a_lhs, E a_rhs) { return a_lhs = (E)((__underlying_type(E))a_lhs & (__underlying_type(E))a_rhs); } \
	inline constexpr E&   operator^=(E& a_lhs, E a_rhs) { return a_lhs = (E)((__underlying_type(E))a_lhs ^ (__underlying_type(E))a_rhs); } \
	inline constexpr E    operator|(E a_lhs, E a_rhs) { return (E)((__underlying_type(E))a_lhs | (__underlying_type(E))a_rhs); }           \
	inline constexpr E    operator&(E a_lhs, E a_rhs) { return (E)((__underlying_type(E))a_lhs & (__underlying_type(E))a_rhs); }           \
	inline constexpr E    operator^(E a_lhs, E a_rhs) { return (E)((__underlying_type(E))a_lhs ^ (__underlying_type(E))a_rhs); }           \
	inline constexpr bool operator!(E a_lhs) { return !(__underlying_type(E))a_lhs; }                                                      \
	inline constexpr E    operator~(E a_lhs) { return (E) ~(__underlying_type(E))a_lhs; }
