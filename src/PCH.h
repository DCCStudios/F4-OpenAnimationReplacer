#pragma once

#pragma warning(push)
#include "F4SE/F4SE.h"
#include "RE/Fallout.h"
#pragma warning(pop)

#pragma warning(disable: 4100)
#pragma warning(disable: 4189)
#pragma warning(disable: 4244)
#pragma warning(disable: 4302)
#pragma warning(disable: 4311)

#define DLLEXPORT __declspec(dllexport)

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <fmt/format.h>

// The multi-runtime CommonLibF4 fork has no F4SE::log wrapper; logger:: call
// sites map 1:1 onto spdlog's free functions (default logger installed by
// InitializeLogging() in main.cpp). Same pattern as F4SE Menu Framework 3.
namespace logger = spdlog;

// Runtime verbose-logging gate. Defined in Settings.cpp (returns
// Settings::GetSingleton()->bVerboseLogging). Declared here — not via Settings.h —
// so the OAR_VLOG macro compiles in every TU without PCH depending on the project's
// Settings header (layering). The global spdlog level is left untouched (LogSetup's
// initial trace level stands); each OAR_VLOG call decides at runtime whether to log,
// so toggling verbose has no process-global side effect.
bool OAR_IsVerboseLogging();

// Log at info ONLY when verbose logging is enabled. Args are evaluated lazily
// (only inside the taken branch), matching a real level check.
#define OAR_VLOG(...)                            \
	do {                                         \
		if (OAR_IsVerboseLogging()) {            \
			::logger::info(__VA_ARGS__);         \
		}                                        \
	} while (0)

using namespace std::literals;

#include <algorithm>
#include <any>
#include <array>
#include <map>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "SimpleIni.h"
