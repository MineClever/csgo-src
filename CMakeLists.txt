cmake_minimum_required(VERSION 3.20)

project(CSGO CXX C)

# -------------------------------------------------------------------------
# 强制 Win32 (x86)
# -------------------------------------------------------------------------
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "必须配置为 32 位 (x86)。\n"
        "请使用: cmake -B build -A Win32")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "仅支持 MSVC 编译器。")
endif()

# -------------------------------------------------------------------------
# 运行时库：全局使用静态 CRT (/MT /MTd)，不用 CMake 默认的 /MD /MDd
# -------------------------------------------------------------------------
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# -------------------------------------------------------------------------
# 路径变量（供子目录引用）
# -------------------------------------------------------------------------
set(REPO_ROOT     "${CMAKE_CURRENT_SOURCE_DIR}")
set(SRC_DIR       "${REPO_ROOT}/src")
set(GAME_BIN_DIR  "${REPO_ROOT}/game/bin"      CACHE PATH "game/bin output directory")
set(GAME_CSGO_BIN "${REPO_ROOT}/game/csgo/bin" CACHE PATH "game/csgo/bin output directory")
set(GAME_DIR      "${REPO_ROOT}/game"           CACHE PATH "game/ directory for EXE output")
set(LIB_PUBLIC    "${SRC_DIR}/lib/public")
set(LIB_COMMON    "${SRC_DIR}/lib/common")

# -------------------------------------------------------------------------
# 默认仅构建 Debug 和 Release
# -------------------------------------------------------------------------
if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()

add_subdirectory(src)
