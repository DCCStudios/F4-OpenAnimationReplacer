#pragma once

#include "REX/BASE.h"

#include "REL/Version.h"
#include "REX/FModuleSection.h"
#include "REX/W32/BASE.h"

#ifndef COMMONLIB_RUNTIMECOUNT
#	define COMMONLIB_RUNTIMECOUNT 3
#endif

namespace REX
{
	class FModule
	{
	public:
		constexpr FModule() = default;

		explicit FModule(W32::HMODULE a_module) :
			m_base(reinterpret_cast<std::uintptr_t>(a_module))
		{}

		enum class Runtime : std::uint8_t
		{
			kOG = 0,
			kNG,
			kAE
		};

		[[nodiscard]] static Runtime GetRuntimeIndex() noexcept;

		[[nodiscard]] inline static bool IsRuntimeOG() noexcept { return GetRuntimeIndex() == Runtime::kOG; }
		[[nodiscard]] inline static bool IsRuntimeNG() noexcept { return GetRuntimeIndex() == Runtime::kNG; }
		[[nodiscard]] inline static bool IsRuntimeAE() noexcept { return GetRuntimeIndex() == Runtime::kAE; }

		static FModule GetCurrentModule();
		static FModule GetExecutingModule();
		static FModule GetLoadedModule(std::string_view a_name);

		[[nodiscard]] std::string GetFileName() const noexcept;

		[[nodiscard]] REL::Version GetFileVersion() const noexcept;

		[[nodiscard]] void* GetExportFunctionPointer(std::string_view a_function) const;

		[[nodiscard]] std::uintptr_t GetExportFunctionAddress(std::string_view a_function) const
		{
			return reinterpret_cast<std::uintptr_t>(GetExportFunctionPointer(a_function));
		}

		[[nodiscard]] void* GetImportFunctionPointer(std::string_view a_function, std::string_view a_library) const;

		[[nodiscard]] std::uintptr_t GetImportFunctionAddress(std::string_view a_function, std::string_view a_library) const
		{
			return reinterpret_cast<std::uintptr_t>(GetImportFunctionPointer(a_function, a_library));
		}

		void* SetImportFunctionPointer(std::string_view a_function, std::string_view a_library, void* a_pointer) const;

		std::uintptr_t SetImportFunctionAddress(std::string_view a_function, std::string_view a_library, std::uintptr_t a_address) const
		{
			return reinterpret_cast<std::uintptr_t>(SetImportFunctionPointer(a_function, a_library, reinterpret_cast<void*>(a_address)));
		}

		[[nodiscard]] FModuleSection GetSection(std::string_view a_section) const;

		[[nodiscard]] constexpr std::uintptr_t GetBaseAddress() const noexcept
		{
			return m_base;
		}

	private:
		std::uintptr_t m_base{ 0 };
	};
}
