#pragma once

#include "REX/BASE.h"

namespace REX
{
	template <class T, class U>
	[[nodiscard]] auto ADJUST_POINTER(U* a_ptr, std::ptrdiff_t a_adjust) noexcept
	{
		auto addr = a_ptr ? reinterpret_cast<std::uintptr_t>(a_ptr) + a_adjust : 0;
		if constexpr (std::is_const_v<U> && std::is_volatile_v<U>) {
			return reinterpret_cast<std::add_cv_t<T>*>(addr);
		} else if constexpr (std::is_const_v<U>) {
			return reinterpret_cast<std::add_const_t<T>*>(addr);
		} else if constexpr (std::is_volatile_v<U>) {
			return reinterpret_cast<std::add_volatile_t<T>*>(addr);
		} else {
			return reinterpret_cast<T*>(addr);
		}
	}

	template <class T>
	void EMPLACE_VTABLE(T* a_ptr)
	{
		reinterpret_cast<std::uintptr_t*>(a_ptr)[0] = T::VTABLE[0].address();
	}

	template <class T>
	void MEM_WRITE_ZERO(volatile T* a_ptr, std::size_t a_size = sizeof(T))
	{
		std::fill_n(reinterpret_cast<volatile char*>(a_ptr), a_size, '\0');
	}

	template <class T1, class T2>
	[[nodiscard]] T1 UNRESTRICTED_CAST(T2 a_from)
	{
		if constexpr (std::is_same_v<
						  std::remove_cv_t<T2>,
						  std::remove_cv_t<T1>>) {
			return T1{ a_from };

			// From != To
		} else if constexpr (std::is_reference_v<T2>) {
			return unrestricted_cast<T1>(std::addressof(a_from));

			// From: NOT reference
		} else if constexpr (std::is_reference_v<T1>) {
			return *unrestricted_cast<
				std::add_pointer_t<
					std::remove_reference_t<T1>>>(a_from);

			// To: NOT reference
		} else if constexpr (std::is_pointer_v<T2> &&
							 std::is_pointer_v<T1>) {
			return static_cast<T1>(
				const_cast<void*>(
					static_cast<const volatile void*>(a_from)));
		} else if constexpr ((std::is_pointer_v<T2> && std::is_integral_v<T1>) ||
							 (std::is_integral_v<T2> && std::is_pointer_v<T1>)) {
			return reinterpret_cast<T1>(a_from);
		} else {
			union
			{
				std::remove_cv_t<std::remove_reference_t<T2>> from;
				std::remove_cv_t<std::remove_reference_t<T1>> to;
			};

			from = std::forward<T2>(a_from);
			return to;
		}
	}
}
