cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(MayaDmxPlugin LANGUAGES CXX)
endif()

option(MAYA_DMX_PREFER_X64 "Prefer x64 builds for the Maya DMX plugin." ON)
option(MAYA_DMX_BUILD_PDB "Generate deployable PDB files for Maya plugin targets." ON)
set(MAYA_DEVKIT_ROOT "D:/_Code_Here/Maya/Autodesk_Maya_2022_5_Update_DEVKIT_Windows/devkitBase" CACHE PATH "Maya 2022.5 DevKit root")
set(MAYA_INSTALL_ROOT "C:/Program Files/Autodesk/Maya2022" CACHE PATH "Maya 2022.5 install root")
set(MAYA_DMX_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin" CACHE PATH "Output directory for Maya DMX plugin binaries")
set(DCC_PLUGIN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT MSVC)
    message(FATAL_ERROR "dcc_plugin currently supports MSVC only.")
endif()

if(NOT WIN32)
    message(FATAL_ERROR "dcc_plugin currently supports Windows only.")
endif()

if(MAYA_DMX_PREFER_X64 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(WARNING "Skipping dcc_plugin because Maya 2022.5 plugin builds are configured to prefer x64. Reconfigure with -A x64 to enable this target.")
    return()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MayaPluginSupport)

maya_dmx_configure_sdk()

add_subdirectory(src/common)
add_subdirectory(src/common_dmx)
add_subdirectory(src/common_smd)
add_subdirectory(src/importer_dmx)
add_subdirectory(src/importer_smd)
add_subdirectory(src/exporter_dmx)
add_subdirectory(src/exporter_smd)
add_subdirectory(src/workflow)
add_subdirectory(src/plugin_dmx)
add_subdirectory(src/plugin_smd)
add_subdirectory(tools)
