#pragma once

#include "REX/BASE.h"

namespace REX
{
	template <class EF>
		requires(std::invocable<std::remove_reference_t<EF>>)
	class TScopeExit
	{
	public:
		template <class Fn>
		explicit TScopeExit(Fn&& a_fn) noexcept(std::is_nothrow_constructible_v<EF, Fn> ||
												std::is_nothrow_constructible_v<EF, Fn&>)
			requires(!std::is_same_v<std::remove_cvref_t<Fn>, TScopeExit> &&
					 std::is_constructible_v<EF, Fn>)
		{
			static_assert(std::invocable<Fn>);

			if constexpr (!std::is_lvalue_reference_v<Fn> &&
						  std::is_nothrow_constructible_v<EF, Fn>) {
				_fn.emplace(std::forward<Fn>(a_fn));
			} else {
				_fn.emplace(a_fn);
			}
		}

		TScopeExit(TScopeExit&& a_rhs) noexcept(std::is_nothrow_move_constructible_v<EF> ||
												std::is_nothrow_copy_constructible_v<EF>)
			requires(std::is_nothrow_move_constructible_v<EF> ||
					 std::is_copy_constructible_v<EF>)
		{
			static_assert(!(std::is_nothrow_move_constructible_v<EF> && !std::is_move_constructible_v<EF>));
			static_assert(!(!std::is_nothrow_move_constructible_v<EF> && !std::is_copy_constructible_v<EF>));

			if (a_rhs.active()) {
				if constexpr (std::is_nothrow_move_constructible_v<EF>) {
					_fn.emplace(std::forward<EF>(*a_rhs._fn));
				} else {
					_fn.emplace(a_rhs._fn);
				}
				a_rhs.release();
			}
		}

		TScopeExit(const TScopeExit&) = delete;

		~TScopeExit() noexcept
		{
			if (_fn.has_value()) {
				(*_fn)();
			}
		}

		void release() noexcept { _fn.reset(); }

	private:
		[[nodiscard]] bool active() const noexcept { return _fn.has_value(); }

		std::optional<std::remove_reference_t<EF>> _fn;
	};

	template <class EF>
	TScopeExit(EF) -> TScopeExit<EF>;
}
