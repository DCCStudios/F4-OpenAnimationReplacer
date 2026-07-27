#include "REX/LOG.h"

#include "REX/W32/KERNEL32.h"
#include "REX/W32/USER32.h"

#include <spdlog/spdlog.h>

namespace REX::Impl
{
	void Log(const std::source_location a_loc, const ELogLevel a_level, const std::string_view a_fmt)
	{
		const auto loc = spdlog::source_loc{ a_loc.file_name(), static_cast<std::int32_t>(a_loc.line()), a_loc.function_name() };
		switch (a_level) {
			case ELogLevel::Trace:
				spdlog::default_logger_raw()->log(loc, spdlog::level::trace, a_fmt);
				break;
			case ELogLevel::Debug:
				spdlog::default_logger_raw()->log(loc, spdlog::level::debug, a_fmt);
				break;
			case ELogLevel::Info:
				spdlog::default_logger_raw()->log(loc, spdlog::level::info, a_fmt);
				break;
			case ELogLevel::Warning:
				spdlog::default_logger_raw()->log(loc, spdlog::level::warn, a_fmt);
				break;
			case ELogLevel::Error:
				spdlog::default_logger_raw()->log(loc, spdlog::level::err, a_fmt);
				break;
			case ELogLevel::Critical:
				spdlog::default_logger_raw()->log(loc, spdlog::level::critical, a_fmt);
				break;
		}
	}

	void Log(const std::source_location a_loc, const ELogLevel a_level, const std::wstring_view a_fmt)
	{
		// vcpkg's spdlog is built without SPDLOG_WCHAR_TO_UTF8_SUPPORT, so the
		// wide overloads don't exist; convert to UTF-8 and use the narrow path.
		std::string narrow;
		if (!a_fmt.empty()) {
			const int size = REX::W32::WideCharToMultiByte(
				65001 /*CP_UTF8*/, 0, a_fmt.data(), static_cast<int>(a_fmt.size()), nullptr, 0, nullptr, nullptr);
			if (size > 0) {
				narrow.resize(static_cast<std::size_t>(size));
				REX::W32::WideCharToMultiByte(
					65001, 0, a_fmt.data(), static_cast<int>(a_fmt.size()), narrow.data(), size, nullptr, nullptr);
			}
		}
		Log(a_loc, a_level, std::string_view{ narrow });
	}
}

namespace REX::Impl
{
	void Fail(const std::source_location a_loc, const std::string_view a_fmt)
	{
		const auto body = [&]() {
			constexpr std::array directories{
				"include/"sv,
				"src/"sv,
			};

			const std::filesystem::path p = a_loc.file_name();
			const auto                  filename = p.generic_string();
			std::string_view            fileview = filename;

			constexpr auto npos = std::string::npos;
			std::size_t    pos = npos;
			std::size_t    off = 0;
			for (const auto& dir : directories) {
				pos = fileview.find(dir);
				if (pos != npos) {
					off = dir.length();
					break;
				}
			}

			if (pos != npos) {
				fileview = fileview.substr(pos + off);
			}

			return std::format("{}({}): {}", fileview, a_loc.line(), a_fmt);
		}();

		const auto caption = []() -> std::string {
			std::vector<char> buf;
			buf.reserve(REX::W32::MAX_PATH);
			buf.resize(REX::W32::MAX_PATH / 2);
			std::uint32_t result = 0;
			do {
				buf.resize(buf.size() * 2);
				result = REX::W32::GetModuleFileNameA(
					REX::W32::GetCurrentModule(),
					buf.data(),
					static_cast<std::uint32_t>(buf.size()));
			} while (result && result == buf.size() && buf.size() <= std::numeric_limits<std::uint32_t>::max());

			if (result && result != buf.size()) {
				std::filesystem::path p(buf.begin(), buf.begin() + result);
				return p.filename().string();
			} else {
				return {};
			}
		}();

		Log(a_loc, ELogLevel::Critical, a_fmt);
		REX::W32::MessageBoxA(nullptr, body.c_str(), (caption.empty() ? nullptr : caption.c_str()), 0);
		REX::W32::TerminateProcess(REX::W32::GetCurrentProcess(), 1);
	}

	void Fail(const std::source_location a_loc, const std::wstring_view a_fmt)
	{
		const auto body = [&]() {
			constexpr std::array directories{
				L"include/"sv,
				L"src/"sv,
			};

			const std::filesystem::path p = a_loc.file_name();
			const auto                  filename = p.generic_wstring();
			std::wstring_view           fileview = filename;

			constexpr auto npos = std::wstring::npos;
			std::size_t    pos = npos;
			std::size_t    off = 0;
			for (const auto& dir : directories) {
				pos = fileview.find(dir);
				if (pos != npos) {
					off = dir.length();
					break;
				}
			}

			if (pos != npos) {
				fileview = fileview.substr(pos + off);
			}

			return std::format(L"{}({}): {}", fileview, a_loc.line(), a_fmt);
		}();

		const auto caption = []() -> std::wstring {
			std::vector<wchar_t> buf;
			buf.reserve(REX::W32::MAX_PATH);
			buf.resize(REX::W32::MAX_PATH / 2);
			std::uint32_t result = 0;
			do {
				buf.resize(buf.size() * 2);
				result = REX::W32::GetModuleFileNameW(
					REX::W32::GetCurrentModule(),
					buf.data(),
					static_cast<std::uint32_t>(buf.size()));
			} while (result && result == buf.size() && buf.size() <= std::numeric_limits<std::uint32_t>::max());

			if (result && result != buf.size()) {
				std::filesystem::path p(buf.begin(), buf.begin() + result);
				return p.filename().wstring();
			} else {
				return {};
			}
		}();

		Log(a_loc, ELogLevel::Critical, a_fmt);
		REX::W32::MessageBoxW(nullptr, body.c_str(), (caption.empty() ? nullptr : caption.c_str()), 0);
		REX::W32::TerminateProcess(REX::W32::GetCurrentProcess(), 1);
	}
}
