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
# Windows SDK 依赖（可配置）
# -------------------------------------------------------------------------
# 默认 SDK 版本（可根据本机安装的 SDK 修改）
set(WINDOWS_SDK_VERSION "10.0.26200" CACHE STRING "Windows SDK version to target (e.g. 10.0.26200)")

# 将 CMake 的系统版本设置为指定的 Windows SDK 版本（Visual Studio generator 会使用此值）
set(CMAKE_SYSTEM_VERSION ${WINDOWS_SDK_VERSION} CACHE STRING "Windows SDK version" FORCE)

# 如果系统环境中存在 WindowsSdkDir，则尝试添加常用 include 路径
# 若未定义，则自动搜索 Windows Kits 的标准安装位置，确保 SDK shared/ 路径
# 在 dx9sdk/include 之前被搜索（解决 ksmedia.h/ks.h 版本冲突问题）

if(DEFINED ENV{WindowsSdkDir})
    set(_win_sdk_root "$ENV{WindowsSdkDir}")
    set(_win_include_um "${_win_sdk_root}/Include/${WINDOWS_SDK_VERSION}/um")
    set(_win_include_shared "${_win_sdk_root}/Include/${WINDOWS_SDK_VERSION}/shared")
    set(_win_include_ucrt "${_win_sdk_root}/Include/${WINDOWS_SDK_VERSION}/ucrt")
else()
    # 自动搜索已安装的 Windows Kits 10，按版本号降序选取最新版
    file(GLOB _winsdk_shared_dirs
        "C:/Program Files (x86)/Windows Kits/10/Include/*/shared"
        "C:/Program Files/Windows Kits/10/Include/*/shared"
    )
    if(_winsdk_shared_dirs)
        list(SORT _winsdk_shared_dirs ORDER DESCENDING)
        list(GET _winsdk_shared_dirs 0 _win_include_shared)
        string(REPLACE "/shared" "/um"   _win_include_um   "${_win_include_shared}")
        string(REPLACE "/shared" "/ucrt" _win_include_ucrt "${_win_include_shared}")
        message(STATUS "自动检测到 Windows SDK shared 路径: ${_win_include_shared}")
    else()
        message(WARNING "未能找到 Windows Kits 10 shared include 路径，若出现 KSJACK_DESCRIPTION3 编译错误请设置 WindowsSdkDir 环境变量。")
    endif()
endif()

# 只将 shared/ 置于 dx9sdk/include 之前，确保新版 ksmedia.h/ks.h 优先被使用。
# 不将 um/ 置于 dx9sdk 之前，因为引擎代码使用 dx9sdk 中的旧版 XAudio2 2.7 API
# (IXAudio2::GetDeviceCount/GetDeviceDetails)，而这些接口在新版 Windows SDK um/xaudio2.h 中已删除。
if(DEFINED _win_include_shared AND EXISTS "${_win_include_shared}")
    include_directories(BEFORE "${_win_include_shared}")
endif()

# 全局链接常用的 Windows 系统库，确保链接阶段能找到 SDK 提供的 .lib
# 这些名字在 MSVC 下会被解析为 e.g. kernel32.lib
link_libraries(kernel32.lib user32.lib gdi32.lib advapi32.lib ws2_32.lib)

# -------------------------------------------------------------------------
# 运行时库：全局使用静态 CRT (/MT /MTd)，不用 CMake 默认的 /MD /MDd
# -------------------------------------------------------------------------
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# -------------------------------------------------------------------------
# 路径变量（供子目录引用）
# -------------------------------------------------------------------------
set(REPO_ROOT     "${CMAKE_CURRENT_SOURCE_DIR}")
set(SRC_DIR       "${REPO_ROOT}/src")
set(UTILS_DIR     "${REPO_ROOT}/src/utils")
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
