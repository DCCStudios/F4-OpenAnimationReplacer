#include "REL/IAT.h"

#include "REX/FModule.h"

namespace REL
{
	std::uintptr_t GetIATAddr(std::string_view a_dll, std::string_view a_function)
	{
		const auto mod = REX::FModule::GetExecutingModule();
		return mod.GetImportFunctionAddress(a_function, a_dll);
	}

	std::uintptr_t GetIATAddr(REX::W32::HMODULE a_module, std::string_view a_dll, std::string_view a_function)
	{
		const auto mod = REX::FModule(a_module);
		return mod.GetImportFunctionAddress(a_function, a_dll);
	}

	void* GetIATPtr(std::string_view a_dll, std::string_view a_function)
	{
		const auto mod = REX::FModule::GetExecutingModule();
		return mod.GetImportFunctionPointer(a_function, a_dll);
	}

	void* GetIATPtr(REX::W32::HMODULE a_module, std::string_view a_dll, std::string_view a_function)
	{
		const auto mod = REX::FModule(a_module);
		return mod.GetImportFunctionPointer(a_function, a_dll);
	}

	std::uintptr_t PatchIAT(std::uintptr_t a_newFunc, std::string_view a_dll, std::string_view a_function)
	{
		const auto mod = REX::FModule::GetExecutingModule();
		return mod.SetImportFunctionAddress(a_function, a_dll, a_newFunc);
	}
}
