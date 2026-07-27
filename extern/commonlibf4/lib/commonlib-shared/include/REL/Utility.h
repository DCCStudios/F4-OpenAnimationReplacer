#pragma once

#include "REX/BASE.h"

namespace REL
{
	void Write(void* a_dst, const void* a_src, const std::size_t a_size);
	void Write(const std::uintptr_t a_dst, const void* a_src, const std::size_t a_size);

	template <class T>
	void Write(void* a_dst, const std::span<T> a_src)
	{
		Write(a_dst, a_src.data(), a_src.size_bytes());
	}

	template <class T>
	void Write(const std::uintptr_t a_dst, const std::span<T> a_src)
	{
		Write(reinterpret_cast<void*>(a_dst), a_src.data(), a_src.size_bytes());
	}

	template <class T>
	void Write(const std::span<T> a_dst, const void* a_src)
	{
		Write(a_dst.data(), a_src, a_dst.size_bytes());
	}

	template <class T>
	void Write(const std::span<T> a_dst, const std::uintptr_t a_src)
	{
		Write(a_dst.data(), reinterpret_cast<void*>(a_src), a_dst.size_bytes());
	}

	template <class T>
	void WriteData(void* a_dst, const T& a_src)
	{
		Write(a_dst, std::addressof(a_src), sizeof(T));
	}

	template <class T>
	void WriteData(const std::uintptr_t a_dst, const T& a_src)
	{
		Write(reinterpret_cast<void*>(a_dst), std::addressof(a_src), sizeof(T));
	}

	template <class T1, class T2>
	void WriteData(const std::span<T1> a_dst, const T2& a_src)
	{
		Write(a_dst.data(), std::addressof(a_src), a_dst.size_bytes());
	}

	bool WriteSafe(void* a_dst, const void* a_src, const std::size_t a_size);
	bool WriteSafe(const std::uintptr_t a_dst, const void* a_src, const std::size_t a_size);
	bool WriteSafeFill(void* a_dst, const std::uint8_t a_value, const std::size_t a_size);
	bool WriteSafeFill(const std::uintptr_t a_dst, const std::uint8_t a_value, const std::size_t a_size);

	template <class T>
	bool WriteSafe(void* a_dst, const std::span<T> a_src)
	{
		return WriteSafe(a_dst, a_src.data(), a_src.size_bytes());
	}

	template <class T>
	bool WriteSafe(const std::uintptr_t a_dst, const std::span<T> a_src)
	{
		return WriteSafe(reinterpret_cast<void*>(a_dst), a_src.data(), a_src.size_bytes());
	}

	template <class T>
	bool WriteSafe(const std::span<T> a_dst, const void* a_src)
	{
		return WriteSafe(a_dst.data(), a_src, a_dst.size_bytes());
	}

	template <class T>
	bool WriteSafe(const std::span<T> a_dst, const std::uintptr_t a_src)
	{
		return WriteSafe(a_dst.data(), reinterpret_cast<void*>(a_src), a_dst.size_bytes());
	}

	template <class T>
	bool WriteSafeData(void* a_dst, const T& a_src)
	{
		return WriteSafe(a_dst, std::addressof(a_src), sizeof(T));
	}

	template <class T>
	bool WriteSafeData(const std::uintptr_t a_dst, const T& a_src)
	{
		return WriteSafe(reinterpret_cast<void*>(a_dst), std::addressof(a_src), sizeof(T));
	}

	template <class T1, class T2>
	bool WriteSafeData(const std::span<T1> a_dst, const T2& a_src)
	{
		return WriteSafe(a_dst.data(), std::addressof(a_src), a_dst.size_bytes());
	}
}
