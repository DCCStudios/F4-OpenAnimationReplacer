// LogSetup.cpp - spdlog default-logger installation for OAR.
//
// Lives in its own translation unit because it needs <ShlObj_core.h>
// (SHGetKnownFolderPath for the Documents folder), and that SDK header
// declares a COM `ICondition` interface that collides with OAR's own
// global-namespace ICondition (BaseConditions.h) if both are visible in
// the same TU. Nothing here includes OAR condition headers.

#include <ShlObj_core.h>

namespace
{
	// Classic F4SE::log::log_directory() replacement (the multi-runtime
	// CommonLibF4 fork has no F4SE::log wrapper): Documents/My Games/Fallout4/F4SE.
	std::optional<std::filesystem::path> GetF4SELogDirectory()
	{
		wchar_t* docsRaw = nullptr;
		if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &docsRaw))) {
			if (docsRaw) ::CoTaskMemFree(docsRaw);
			return std::nullopt;
		}
		std::filesystem::path docs(docsRaw);
		::CoTaskMemFree(docsRaw);
		return docs / "My Games" / "Fallout4" / "F4SE";
	}
}

void OAR_InitializeLogging(std::string_view a_pluginName)
{
	auto path = GetF4SELogDirectory();
	if (!path) {
		REX::FAIL("Failed to find F4SE log directory");
	}
	std::error_code ec;
	std::filesystem::create_directories(*path, ec);
	*path /= std::format("{}.log"sv, a_pluginName);

	auto log = std::make_shared<spdlog::logger>(
		"global"s,
		std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
	log->set_level(spdlog::level::trace);
	// Flushing every info line costs a syscall per message — with per-file
	// logging during the animation preload that alone froze the main menu for
	// seconds on large setups. Warnings/errors still flush immediately (crash
	// forensics); info lines are flushed by the periodic flusher below and by
	// explicit flushes at the end of each load phase.
	log->flush_on(spdlog::level::warn);
	set_default_logger(std::move(log));
	spdlog::flush_every(std::chrono::seconds(3));
}
