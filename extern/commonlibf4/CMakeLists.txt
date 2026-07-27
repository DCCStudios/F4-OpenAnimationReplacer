# CMake shim for the Dear-Modding multi-runtime CommonLibF4 fork.
#
# Upstream builds with xmake only (see xmake.lua); this file mirrors its two
# targets so the plugin's existing CMake + vcpkg toolchain can consume them:
#   commonlib-shared  - REL/REX runtime layer (module/version detection,
#                       address-library IDDB, trampoline, W32 shims)
#   CommonLibF4       - the RE/F4SE/Scaleform reverse-engineered headers + impl
#
# COMMONLIB_RUNTIMECOUNT=3 gives every REL::ID three slots resolved at runtime
# by REX::FModule::GetRuntimeIndex():
#   [0] OG  = exactly 1.10.163
#   [1] NG  = >1.10.163 and <=1.10.984
#   [2] AE  = >1.10.984 (1.11.137 ... 1.11.221+)
#
# The optional REX setting stores (INI/JSON/TOML) and xbyak trampoline support
# stay off - the plugin does not use them.

cmake_minimum_required(VERSION 3.21)
project(commonlibf4 LANGUAGES CXX)

find_package(spdlog REQUIRED CONFIG)

# ---------------------------------------------------------------- #
#  commonlib-shared                                                #
# ---------------------------------------------------------------- #

file(GLOB_RECURSE commonlib_shared_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlib-shared/src/*.cpp)

add_library(commonlib-shared STATIC ${commonlib_shared_sources})

target_compile_features(commonlib-shared PUBLIC cxx_std_23)
target_include_directories(commonlib-shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlib-shared/include)
target_compile_definitions(commonlib-shared PUBLIC
    COMMONLIB_RUNTIMECOUNT=3
    NOMINMAX
    WIN32_LEAN_AND_MEAN)
target_link_libraries(commonlib-shared PUBLIC spdlog::spdlog)
target_link_libraries(commonlib-shared INTERFACE
    advapi32 bcrypt d3d11 d3dcompiler dbghelp dxgi ole32 shell32 user32 version ws2_32)
target_precompile_headers(commonlib-shared PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlib-shared/src/REX/PCH.h)

if (MSVC)
    target_compile_options(commonlib-shared PRIVATE
        /EHsc /permissive- /bigobj /utf-8
        /Zc:preprocessor /Zc:templateScope /Zc:enumTypes
        /wd4200 /wd4201 /wd4324)
endif()

# ---------------------------------------------------------------- #
#  CommonLibF4 (RE / F4SE / Scaleform)                             #
# ---------------------------------------------------------------- #

file(GLOB_RECURSE commonlibf4_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(CommonLibF4 STATIC ${commonlibf4_sources})
add_library(CommonLibF4::CommonLibF4 ALIAS CommonLibF4)

target_compile_features(CommonLibF4 PUBLIC cxx_std_23)
target_include_directories(CommonLibF4 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(CommonLibF4 PUBLIC commonlib-shared)
target_precompile_headers(CommonLibF4 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/F4SE/Impl/PCH.h)

if (MSVC)
    target_compile_options(CommonLibF4 PRIVATE
        /EHsc /permissive- /bigobj /utf-8
        /Zc:preprocessor /Zc:templateScope /Zc:enumTypes
        /wd4200 /wd4201 /wd4324)
endif()
