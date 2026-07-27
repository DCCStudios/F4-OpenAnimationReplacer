#pragma once

#include "REX/BASE.h"

namespace REX
{
	template <class T>
	class TAtomicRef :
		public std::atomic_ref<T>
	{
	private:
		using super = std::atomic_ref<T>;

	public:
		using value_type = typename super::value_type;

		explicit TAtomicRef(volatile T& a_obj) noexcept(std::is_nothrow_constructible_v<super, value_type&>) :
			super(const_cast<value_type&>(a_obj))
		{}

		using super::super;
		using super::operator=;
	};

	template <class T>
	TAtomicRef(volatile T&) -> TAtomicRef<T>;

	template class TAtomicRef<std::int8_t>;
	template class TAtomicRef<std::uint8_t>;
	template class TAtomicRef<std::int16_t>;
	template class TAtomicRef<std::uint16_t>;
	template class TAtomicRef<std::int32_t>;
	template class TAtomicRef<std::uint32_t>;
	template class TAtomicRef<std::int64_t>;
	template class TAtomicRef<std::uint64_t>;

	static_assert(TAtomicRef<std::int8_t>::is_always_lock_free);
	static_assert(TAtomicRef<std::uint8_t>::is_always_lock_free);
	static_assert(TAtomicRef<std::int16_t>::is_always_lock_free);
	static_assert(TAtomicRef<std::uint16_t>::is_always_lock_free);
	static_assert(TAtomicRef<std::int32_t>::is_always_lock_free);
	static_assert(TAtomicRef<std::uint32_t>::is_always_lock_free);
	static_assert(TAtomicRef<std::int64_t>::is_always_lock_free);
	static_assert(TAtomicRef<std::uint64_t>::is_always_lock_free);
}
