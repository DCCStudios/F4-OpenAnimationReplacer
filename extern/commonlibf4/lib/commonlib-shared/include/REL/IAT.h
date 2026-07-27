#pragma once

#include "REX/BASE.h"

#include "REX/CAST.h"
#include "REX/W32/BASE.h"

namespace REL
{
	[[nodiscard, deprecated("Use 'REX::FModule' instead")]] std::uintptr_t GetIATAddr(std::string_view a_dll, std::string_view a_function);
	[[nodiscard, deprecated("Use 'REX::FModule' instead")]] std::uintptr_t GetIATAddr(REX::W32::HMODULE a_module, std::string_view a_dll, std::string_view a_function);

	[[nodiscard, deprecated("Use 'REX::FModule' instead")]] void* GetIATPtr(std::string_view a_dll, std::string_view a_function);

	template <class T>
	[[nodiscard, deprecated("Use 'REX::FModule' instead")]] T* GetIATPtr(std::string_view a_dll, std::string_view a_function)
	{
		return static_cast<T*>(GetIATPtr(std::move(a_dll), std::move(a_function)));
	}

	[[nodiscard, deprecated("Use 'REX::FModule' instead")]] void* GetIATPtr(REX::W32::HMODULE a_module, std::string_view a_dll, std::string_view a_function);

	template <class T>
	[[nodiscard, deprecated("Use 'REX::FModule' instead")]] T* GetIATPtr(REX::W32::HMODULE a_module, std::string_view a_dll, std::string_view a_function)
	{
		return static_cast<T*>(GetIATPtr(a_module, std::move(a_dll), std::move(a_function)));
	}

	[[deprecated("Use 'REX::FModule' instead")]] std::uintptr_t PatchIAT(std::uintptr_t a_newFunc, std::string_view a_dll, std::string_view a_function);

	template <class F>
	[[deprecated("Use 'REX::FModule' instead")]] std::uintptr_t PatchIAT(F a_newFunc, std::string_view a_dll, std::string_view a_function)
	{
		return PatchIAT(REX::UNRESTRICTED_CAST<std::uintptr_t>(a_newFunc), a_dll, a_function);
	}
}
