# Shared top-level CMake setup for the main workspace and VPC-only solution.

if(NOT DEFINED REPO_ROOT)
    set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "仅支持 MSVC 编译器。")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(VALVE_TARGET_ARCH "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(VALVE_TARGET_ARCH "x86")
else()
    message(FATAL_ERROR "Unsupported pointer size: ${CMAKE_SIZEOF_VOID_P}")
endif()

message(STATUS "Configuring for target architecture: ${VALVE_TARGET_ARCH}")

set(WINDOWS_SDK_VERSION "10.0.26200" CACHE STRING "Windows SDK version to target (e.g. 10.0.26200)")
set(CMAKE_SYSTEM_VERSION ${WINDOWS_SDK_VERSION} CACHE STRING "Windows SDK version" FORCE)

if(DEFINED ENV{WindowsSdkDir})
    set(_win_sdk_root "$ENV{WindowsSdkDir}")
    set(_win_include_um "${_win_sdk_root}/Include/${WINDOWS_SDK_VERSION}/um")
    set(_win_include_shared "${_win_sdk_root}/Include/${WINDOWS_SDK_VERSION}/shared")
    set(_win_include_ucrt "${_win_sdk_root}/Include/${WINDOWS_SDK_VERSION}/ucrt")
else()
    file(GLOB _winsdk_shared_dirs
        "C:/Program Files (x86)/Windows Kits/10/Include/*/shared"
        "C:/Program Files/Windows Kits/10/Include/*/shared"
    )
    if(_winsdk_shared_dirs)
        list(SORT _winsdk_shared_dirs ORDER DESCENDING)
        list(GET _winsdk_shared_dirs 0 _win_include_shared)
        string(REPLACE "/shared" "/um" _win_include_um "${_win_include_shared}")
        string(REPLACE "/shared" "/ucrt" _win_include_ucrt "${_win_include_shared}")
        message(STATUS "自动检测到 Windows SDK shared 路径: ${_win_include_shared}")
    else()
        message(WARNING "未能找到 Windows Kits 10 shared include 路径，若出现 KSJACK_DESCRIPTION3 编译错误请设置 WindowsSdkDir 环境变量。")
    endif()
endif()

if(DEFINED _win_include_shared AND EXISTS "${_win_include_shared}")
    include_directories(BEFORE "${_win_include_shared}")
endif()

link_libraries(kernel32.lib user32.lib gdi32.lib advapi32.lib ws2_32.lib)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(SRC_DIR "${REPO_ROOT}/src")
set(UTILS_DIR "${REPO_ROOT}/src/utils")
set(GAME_BIN_DIR "${REPO_ROOT}/game/bin" CACHE PATH "game/bin output directory")
set(GAME_CSGO_BIN "${REPO_ROOT}/game/csgo/bin" CACHE PATH "game/csgo/bin output directory")
set(GAME_DIR "${REPO_ROOT}/game" CACHE PATH "game/ directory for EXE output")
set(DEVTOOLS_BIN_DIR "${SRC_DIR}/devtools/bin" CACHE PATH "src/devtools/bin output directory")
set(LIB_PUBLIC "${SRC_DIR}/lib/public")
set(VALVE_BUILT_LIB_PUBLIC "${LIB_PUBLIC}/${VALVE_TARGET_ARCH}")
set(LIB_COMMON "${SRC_DIR}/lib/common")

set(_valve_fbx_sdk_env "")
foreach(_valve_fbx_env_var FBX_SDK_DIR FBXSDK_DIR FBX_SDK FBXSDK)
    if(DEFINED ENV{${_valve_fbx_env_var}} AND NOT "$ENV{${_valve_fbx_env_var}}" STREQUAL "")
        set(_valve_fbx_sdk_env "$ENV{${_valve_fbx_env_var}}")
        break()
    endif()
endforeach()

set(_valve_fbx_sdk_default "D:/_Code_Here/FbxPythonBindings/FBX SDK/2020.3.7")
if(NOT _valve_fbx_sdk_env STREQUAL "")
    set(_valve_fbx_sdk_default "${_valve_fbx_sdk_env}")
endif()

set(VALVE_FBX_SDK_DIR "${_valve_fbx_sdk_default}" CACHE PATH "Autodesk FBX SDK root")
set(VALVE_FBX_INCLUDE_DIR "${VALVE_FBX_SDK_DIR}/include" CACHE PATH "Autodesk FBX SDK include dir")

if(VALVE_TARGET_ARCH STREQUAL "x64")
    set(VALVE_FBX_ARCH_DIR "x64")
else()
    set(VALVE_FBX_ARCH_DIR "x86")
endif()

set(VALVE_FBX_LIB_DEBUG_CANDIDATES
    "${VALVE_FBX_SDK_DIR}/lib/${VALVE_FBX_ARCH_DIR}/debug/libfbxsdk-mt.lib"
    "${VALVE_FBX_SDK_DIR}/lib/${VALVE_FBX_ARCH_DIR}/debug/libfbxsdk.lib"
)
set(VALVE_FBX_LIB_RELEASE_CANDIDATES
    "${VALVE_FBX_SDK_DIR}/lib/${VALVE_FBX_ARCH_DIR}/release/libfbxsdk-mt.lib"
    "${VALVE_FBX_SDK_DIR}/lib/${VALVE_FBX_ARCH_DIR}/release/libfbxsdk.lib"
)

unset(VALVE_FBX_LIB_DEBUG CACHE)
unset(VALVE_FBX_LIB_RELEASE CACHE)

foreach(_valve_fbx_lib IN LISTS VALVE_FBX_LIB_DEBUG_CANDIDATES)
    if(NOT DEFINED VALVE_FBX_LIB_DEBUG AND EXISTS "${_valve_fbx_lib}")
        set(VALVE_FBX_LIB_DEBUG "${_valve_fbx_lib}")
    endif()
endforeach()

foreach(_valve_fbx_lib IN LISTS VALVE_FBX_LIB_RELEASE_CANDIDATES)
    if(NOT DEFINED VALVE_FBX_LIB_RELEASE AND EXISTS "${_valve_fbx_lib}")
        set(VALVE_FBX_LIB_RELEASE "${_valve_fbx_lib}")
    endif()
endforeach()

if(EXISTS "${VALVE_FBX_INCLUDE_DIR}/fbxsdk.h" AND DEFINED VALVE_FBX_LIB_DEBUG AND DEFINED VALVE_FBX_LIB_RELEASE)
    set(VALVE_FBX_SDK_AVAILABLE ON)
else()
    set(VALVE_FBX_SDK_AVAILABLE OFF)
endif()

if(VALVE_FBX_SDK_AVAILABLE AND NOT TARGET valve_fbx_sdk)
    add_library(valve_fbx_sdk INTERFACE IMPORTED)
    target_include_directories(valve_fbx_sdk INTERFACE "${VALVE_FBX_INCLUDE_DIR}")
    target_compile_definitions(valve_fbx_sdk INTERFACE FBXSDK_NEW_API FBXSDK_SHARED)
    target_link_libraries(valve_fbx_sdk INTERFACE
        "$<$<CONFIG:Debug>:${VALVE_FBX_LIB_DEBUG}>"
        "$<$<NOT:$<CONFIG:Debug>>:${VALVE_FBX_LIB_RELEASE}>"
    )
elseif(NOT VALVE_FBX_SDK_AVAILABLE)
    message(STATUS "Autodesk FBX SDK unavailable for this toolchain. Checked ${VALVE_FBX_SDK_DIR} with arch ${VALVE_FBX_ARCH_DIR}.")
endif()

set(VALVE_OPENFBX_DIR "${REPO_ROOT}/thirdparty/OpenFBX")
set(VALVE_OPENFBX_AVAILABLE OFF)
if(EXISTS "${VALVE_OPENFBX_DIR}/src/ofbx.cpp" AND EXISTS "${VALVE_OPENFBX_DIR}/src/libdeflate.c" AND NOT TARGET valve_openfbx)
    add_library(valve_openfbx STATIC
        "${VALVE_OPENFBX_DIR}/src/ofbx.cpp"
        "${VALVE_OPENFBX_DIR}/src/libdeflate.c"
    )
    target_include_directories(valve_openfbx PUBLIC "${VALVE_OPENFBX_DIR}/src")
    target_compile_definitions(valve_openfbx PRIVATE _LARGEFILE64_SOURCE)
endif()
if(TARGET valve_openfbx)
    set(VALVE_OPENFBX_AVAILABLE ON)
endif()

set(VALVE_FBX_BACKEND "auto" CACHE STRING "FBX backend to use: auto, sdk, openfbx")
set_property(CACHE VALVE_FBX_BACKEND PROPERTY STRINGS auto sdk openfbx)

set(VALVE_FBX_BACKEND_SDK OFF)
set(VALVE_FBX_BACKEND_OPENFBX OFF)
set(VALVE_FBX_AVAILABLE OFF)

if(VALVE_FBX_BACKEND STREQUAL "auto")
    if(VALVE_FBX_SDK_AVAILABLE)
        set(VALVE_FBX_BACKEND_SDK ON)
    elseif(VALVE_OPENFBX_AVAILABLE)
        set(VALVE_FBX_BACKEND_OPENFBX ON)
    endif()
elseif(VALVE_FBX_BACKEND STREQUAL "sdk")
    if(VALVE_FBX_SDK_AVAILABLE)
        set(VALVE_FBX_BACKEND_SDK ON)
    else()
        message(FATAL_ERROR "VALVE_FBX_BACKEND=sdk but Autodesk FBX SDK is unavailable for ${VALVE_FBX_ARCH_DIR}.")
    endif()
elseif(VALVE_FBX_BACKEND STREQUAL "openfbx")
    if(VALVE_OPENFBX_AVAILABLE)
        set(VALVE_FBX_BACKEND_OPENFBX ON)
    else()
        message(FATAL_ERROR "VALVE_FBX_BACKEND=openfbx but thirdparty/OpenFBX is unavailable.")
    endif()
else()
    message(FATAL_ERROR "Unsupported VALVE_FBX_BACKEND=${VALVE_FBX_BACKEND}. Expected auto, sdk, or openfbx.")
endif()

if(VALVE_FBX_BACKEND_SDK OR VALVE_FBX_BACKEND_OPENFBX)
    set(VALVE_FBX_AVAILABLE ON)
endif()

if(VALVE_FBX_BACKEND_SDK)
    message(STATUS "FBX backend: Autodesk FBX SDK")
elseif(VALVE_FBX_BACKEND_OPENFBX)
    message(STATUS "FBX backend: OpenFBX")
else()
    message(STATUS "FBX backend: unavailable")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()
