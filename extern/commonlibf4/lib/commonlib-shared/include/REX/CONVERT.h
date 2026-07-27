#pragma once

#include "REX/BASE.h"
#include "REX/W32/KERNEL32.h"

namespace REX
{
	template <class U, class... T>
	U BITS_TO_VALUE(T... a_args)
		requires(std::same_as<std::remove_cv_t<T>, bool> && ...)
	{
		constexpr auto ARGC = sizeof...(T);

		std::bitset<ARGC> bits;
		std::size_t       i = 0;
		((bits[i++] = a_args), ...);

		if constexpr (ARGC <= std::numeric_limits<std::uint32_t>::digits) {
			return static_cast<U>(bits.to_ulong());
		} else if constexpr (ARGC <= std::numeric_limits<std::uint64_t>::digits) {
			return static_cast<U>(bits.to_ullong());
		} else {
			static_assert(false && sizeof...(T));
		}
	}

	inline bool UTF8_TO_UTF16(const std::string_view a_in, std::wstring& a_out) noexcept
	{
		const auto cvt = [&](wchar_t* a_dst, std::size_t a_length) {
			return REX::W32::MultiByteToWideChar(
				REX::W32::CP_UTF8,
				0,
				a_in.data(),
				static_cast<int>(a_in.length()),
				a_dst,
				static_cast<int>(a_length));
		};

		const auto len = cvt(nullptr, 0);
		if (len == 0) {
			return false;
		}

		std::wstring out(len, '\0');
		if (cvt(out.data(), out.length()) == 0) {
			return false;
		}

		a_out = out;
		return true;
	}

	inline bool UTF16_TO_UTF8(const std::wstring_view a_in, std::string& a_out) noexcept
	{
		const auto cvt = [&](char* a_dst, std::size_t a_length) {
			return REX::W32::WideCharToMultiByte(
				REX::W32::CP_UTF8,
				0,
				a_in.data(),
				static_cast<int>(a_in.length()),
				a_dst,
				static_cast<int>(a_length),
				nullptr,
				nullptr);
		};

		const auto len = cvt(nullptr, 0);
		if (len == 0) {
			return false;
		}

		std::string out(len, '\0');
		if (cvt(out.data(), out.length()) == 0) {
			return false;
		}

		a_out = out;
		return true;
	}
}
