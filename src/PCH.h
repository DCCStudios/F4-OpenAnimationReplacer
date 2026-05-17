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

#include <spdlog/sinks/basic_file_sink.h>

namespace logger = F4SE::log;

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
