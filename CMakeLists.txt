cmake_minimum_required(VERSION 3.21)

# vcpkg setup
macro(set_from_environment VARIABLE)
	if (NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif ()
endmacro()

set_from_environment(VCPKG_ROOT)
set_from_environment(Fallout4Path)

if (DEFINED VCPKG_ROOT)
	set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
	set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")
else()
	message(WARNING "VCPKG_ROOT is not set. Continuing without vcpkg.")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "")

project(
	OpenAnimationReplacer
	VERSION 1.0.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Compile/F4SE/Plugins")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Compile/F4SE/Plugins")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Compile/F4SE/Plugins")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Compile/F4SE/Plugins")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/Compile/F4SE/Plugins")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/Compile/F4SE/Plugins")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../PluginTemplate/CommonLibF4/cmake")

if (NOT TARGET CommonLibF4)
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../PluginTemplate/CommonLibF4/CommonLibF4" CommonLibF4)
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog REQUIRED CONFIG)
find_package(imgui CONFIG REQUIRED)

set(SOURCES
	src/PCH.cpp
	src/main.cpp
	src/Settings.cpp
	src/Offsets.h
	src/HavokTypes.h
	src/OpenAnimationReplacer.cpp
	src/Hooks.cpp
	src/AnimationCache.cpp
	src/ReplacerMods.cpp
	src/ReplacementAnimation.cpp
	src/ActiveClip.cpp
	src/FakeClipGenerator.cpp
	src/HavokGraphUtils.cpp
	src/BaseConditions.cpp
	src/Conditions.cpp
	src/Parsing.cpp
	src/Variants.cpp
	src/Functions.cpp
	src/Jobs.cpp
	src/Utils.cpp
	src/AnimationLog.cpp
	src/UI/UIManager.cpp
	src/UI/UIMain.cpp
	src/UI/UICommon.cpp
	src/UI/UIWindow.cpp
	src/UI/UIAnimationLog.cpp
	src/UI/UIAnimationEventLog.cpp
	src/UI/UIWelcomeBanner.cpp
	src/UI/UIAnimationQueue.cpp
	src/UI/UIDebugOverlay.cpp
	src/UI/UIFormPicker.cpp
	src/FormRegistry.cpp
	src/API/OpenAnimationReplacerAPI.cpp
)

set(HEADERS
	src/PCH.h
	src/Settings.h
	src/Offsets.h
	src/HavokTypes.h
	src/OpenAnimationReplacer.h
	src/Hooks.h
	src/AnimationCache.h
	src/ReplacerMods.h
	src/ReplacementAnimation.h
	src/ActiveClip.h
	src/FakeClipGenerator.h
	src/HavokGraphUtils.h
	src/BaseConditions.h
	src/Conditions.h
	src/Parsing.h
	src/Variants.h
	src/Jobs.h
	src/Utils.h
	src/AnimationLog.h
	src/SimpleIni.h
	src/UI/UIManager.h
	src/UI/UIMain.h
	src/UI/UICommon.h
	src/UI/UIWindow.h
	src/UI/UIAnimationLog.h
	src/UI/UIAnimationEventLog.h
	src/UI/UIWelcomeBanner.h
	src/UI/UIAnimationQueue.h
	src/UI/UIDebugOverlay.h
	src/UI/UIFormPicker.h
	src/FormRegistry.h
	src/ActiveReplacementTracker.h
	src/RE_Additions.h
	src/API/OpenAnimationReplacerAPI.h
)

add_library(${PROJECT_NAME} SHARED
	${SOURCES}
	${HEADERS}
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	_UNICODE
	UNICODE
	NOMINMAX
	_CRT_SECURE_NO_WARNINGS
	IMGUI_DISABLE_OBSOLETE_FUNCTIONS
	OAR_EXPORTS
)

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	CommonLibF4::CommonLibF4
	nlohmann_json::nlohmann_json
	spdlog::spdlog
	imgui::imgui
	d3d11
	dxgi
)

target_precompile_headers(${PROJECT_NAME} PRIVATE
	src/PCH.h
)

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE
		/sdl
		/utf-8
		/Zi
		/permissive-
		/Zc:preprocessor
		/EHsc
		/W4
		/WX-
		/wd4099
		/wd4100
		/wd4189
		/wd4244
		/wd4302
		/wd4311
		/wd5054
	)

	target_link_options(${PROJECT_NAME} PRIVATE
		"$<$<CONFIG:DEBUG>:/INCREMENTAL>"
		"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif()

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/OpenAnimationReplacer.ini
	${CMAKE_SOURCE_DIR}/Compile/F4SE/Plugins/OpenAnimationReplacer.ini
	COPYONLY
)

option(COPY_BUILD "Copy the build output to the Fallout 4 directory." OFF)
if (COPY_BUILD AND DEFINED Fallout4Path)
	add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${Fallout4Path}/Data/F4SE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "${Fallout4Path}/Data/F4SE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/OpenAnimationReplacer.ini" "${Fallout4Path}/Data/F4SE/Plugins/"
	)
endif()
