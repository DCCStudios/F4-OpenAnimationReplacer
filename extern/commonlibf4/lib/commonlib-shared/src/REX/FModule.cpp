#include "REX/FModule.h"

#include "REL/Utility.h"
#include "REX/CAST.h"
#include "REX/LOG.h"
#include "REX/W32/KERNEL32.h"

namespace REX
{
	constexpr inline static REL::Version OG_LATEST_VERSION = { 1, 10, 163, 0 };
	constexpr inline static REL::Version NG_LATEST_VERSION = { 1, 10, 984, 0 };

	static REL::Version safe_verm{ 0, 0, 0, 0 };

	[[nodiscard]] FModule::Runtime FModule::GetRuntimeIndex() noexcept
	{
		if (safe_verm.major() == 0)
		{
			const auto mod = REX::FModule::GetExecutingModule();
			safe_verm = mod.GetFileVersion();
		}
	
		if (safe_verm == OG_LATEST_VERSION)
			return FModule::Runtime::kOG;
		if ((safe_verm > OG_LATEST_VERSION) && (safe_verm <= NG_LATEST_VERSION))
			return FModule::Runtime::kNG;
		if (safe_verm > NG_LATEST_VERSION)
			return FModule::Runtime::kAE;

		// Default to AE
		return FModule::Runtime::kAE;
	}

	FModule FModule::GetCurrentModule()
	{
		return FModule{ W32::GetCurrentModule() };
	}

	FModule FModule::GetExecutingModule()
	{
		return FModule{ W32::GetModuleHandleA(nullptr) };
	}

	FModule FModule::GetLoadedModule(std::string_view a_name)
	{
		return FModule{ W32::GetModuleHandleA(a_name.data()) };
	}

	std::string FModule::GetFileName() const noexcept
	{
		char path[W32::MAX_PATH];
		if (!W32::GetModuleFileNameA(reinterpret_cast<W32::HMODULE>(m_base), path, W32::MAX_PATH)) {
			REX::FAIL("failed to obtain module file name");
		}

		return std::filesystem::path(path).string();
	}

	REL::Version FModule::GetFileVersion() const noexcept
	{
		return *REL::GetFileVersion(GetFileName());
	}

	void* FModule::GetExportFunctionPointer(std::string_view a_function) const
	{
		if (m_base) {
			return W32::GetProcAddress(reinterpret_cast<W32::HMODULE>(m_base), a_function.data());
		}

		return nullptr;
	}

	void* FModule::GetImportFunctionPointer(std::string_view a_function, std::string_view a_library) const
	{
		const auto dosHeader = reinterpret_cast<W32::IMAGE_DOS_HEADER*>(m_base);
		if (dosHeader->magic != W32::IMAGE_DOS_SIGNATURE) {
			REX::ERROR("Invalid IMAGE_DOS_HEADER");
			return nullptr;
		}

		const auto  ntHeader = ADJUST_POINTER<W32::IMAGE_NT_HEADERS64>(dosHeader, dosHeader->lfanew);
		const auto& dataDir = ntHeader->optionalHeader.dataDirectory[REX::W32::IMAGE_DIRECTORY_ENTRY_IMPORT];
		const auto  importDesc = ADJUST_POINTER<W32::IMAGE_IMPORT_DESCRIPTOR>(dosHeader, dataDir.virtualAddress);

		for (auto import = importDesc; import->characteristics != 0; ++import) {
			auto name = ADJUST_POINTER<const char>(dosHeader, import->name);
			if (a_library.size() == strlen(name) && _strnicmp(a_library.data(), name, a_library.size()) != 0) {
				continue;
			}

			const auto thunk = ADJUST_POINTER<W32::IMAGE_THUNK_DATA64>(dosHeader, import->firstThunkOriginal);
			for (std::size_t i = 0; thunk[i].ordinal; ++i) {
				if (W32::IMAGE_SNAP_BY_ORDINAL64(thunk[i].ordinal)) {
					continue;
				}

				const auto importByName = ADJUST_POINTER<W32::IMAGE_IMPORT_BY_NAME>(dosHeader, thunk[i].address);
				if (a_function.size() == strlen(importByName->name) &&
					_strnicmp(a_function.data(), importByName->name, a_function.size()) == 0) {
					return ADJUST_POINTER<W32::IMAGE_THUNK_DATA64>(dosHeader, import->firstThunk) + i;
				}
			}
		}

		REX::ERROR("Failed to get {} ({})", a_function, a_library);

		return nullptr;
	}

	void* FModule::SetImportFunctionPointer(std::string_view a_function, std::string_view a_library, void* a_pointer) const
	{
		auto original = GetImportFunctionPointer(a_function, a_library);
		if (original) {
			REL::WriteSafeData(original, a_pointer);
		} else {
			REX::ERROR("Failed to set {} ({})", a_function, a_library);
		}

		return original;
	}

	FModuleSection FModule::GetSection(std::string_view a_section) const
	{
		const auto dosHeader = reinterpret_cast<const W32::IMAGE_DOS_HEADER*>(m_base);
		const auto ntHeader = ADJUST_POINTER<W32::IMAGE_NT_HEADERS64>(dosHeader, dosHeader->lfanew);
		const auto sections = W32::IMAGE_FIRST_SECTION(ntHeader);
		for (std::size_t i = 0; i < ntHeader->fileHeader.sectionCount; ++i) {
			const auto&    section = sections[i];
			constexpr auto size = std::extent_v<decltype(section.name)>;
			const auto     len = std::min(a_section.size(), size);

			if (std::memcmp(a_section.data(), section.name, len) == 0) {
				return FModuleSection{ m_base, m_base + section.virtualAddress, section.virtualSize };
			}
		}

		return {};
	}
}
