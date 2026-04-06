# utils/dx_proxy
# Generated from: dx_proxy_dx10_v00_pc.vpc, dx_proxy_base.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(UTILS_DX_PROXY_DX_PROXY_DX_SDK_VER_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/dx_proxy/dx_proxy.cpp
    ${SRC_DIR}/utils/dx_proxy/filememcache.cpp
    ${SRC_DIR}/common/dx_proxy/dx_proxy.h
    ${SRC_DIR}/utils/dx_proxy/dxincludeimpl.h
    ${SRC_DIR}/utils/dx_proxy/filememcache.h
)

add_library(utils_dx_proxy_DX_Proxy_DX_SDK_VER SHARED ${UTILS_DX_PROXY_DX_PROXY_DX_SDK_VER_SOURCES})

valve_apply_base_settings(utils_dx_proxy_DX_Proxy_DX_SDK_VER)

target_include_directories(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    DX_PROXY_EXPORTS
)

target_link_libraries(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    tier0
    vstdlib
)

valve_set_dll_importlib_output(utils_dx_proxy_DX_Proxy_DX_SDK_VER)
valve_publish_dll(utils_dx_proxy_DX_Proxy_DX_SDK_VER "${GAME_BIN_DIR}")

# utils/dx_proxy
# Generated from: dx_proxy_dx9_v00_pc.vpc, dx_proxy_base.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(UTILS_DX_PROXY_DX_PROXY_DX_SDK_VER_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/dx_proxy/dx_proxy.cpp
    ${SRC_DIR}/utils/dx_proxy/filememcache.cpp
    ${SRC_DIR}/common/dx_proxy/dx_proxy.h
    ${SRC_DIR}/utils/dx_proxy/dxincludeimpl.h
    ${SRC_DIR}/utils/dx_proxy/filememcache.h
)

add_library(utils_dx_proxy_DX_Proxy_DX_SDK_VER SHARED ${UTILS_DX_PROXY_DX_PROXY_DX_SDK_VER_SOURCES})

valve_apply_base_settings(utils_dx_proxy_DX_Proxy_DX_SDK_VER)

target_include_directories(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    DX_PROXY_EXPORTS
)

target_link_libraries(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    tier0
    vstdlib
)

valve_set_dll_importlib_output(utils_dx_proxy_DX_Proxy_DX_SDK_VER)
valve_publish_dll(utils_dx_proxy_DX_Proxy_DX_SDK_VER "${GAME_BIN_DIR}")

# utils/dx_proxy
# Generated from: dx_proxy_dx9_v00_x360.vpc, dx_proxy_base.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(UTILS_DX_PROXY_DX_PROXY_DX_SDK_VER_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/dx_proxy/dx_proxy.cpp
    ${SRC_DIR}/utils/dx_proxy/filememcache.cpp
    ${SRC_DIR}/common/dx_proxy/dx_proxy.h
    ${SRC_DIR}/utils/dx_proxy/dxincludeimpl.h
    ${SRC_DIR}/utils/dx_proxy/filememcache.h
)

add_library(utils_dx_proxy_DX_Proxy_DX_SDK_VER SHARED ${UTILS_DX_PROXY_DX_PROXY_DX_SDK_VER_SOURCES})

valve_apply_base_settings(utils_dx_proxy_DX_Proxy_DX_SDK_VER)

target_include_directories(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    $(XEDK)/include/win32
)

target_compile_definitions(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    DX_PROXY_EXPORTS
    _HAS_ITERATOR_DEBUGGING=1
)

target_link_libraries(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    tier0
    vstdlib
    "${SRC_DIR}/utils/dx_proxy/d3d9d.lib"
    "${SRC_DIR}/utils/dx_proxy/d3dx9d.lib"
    "${SRC_DIR}/utils/dx_proxy/xgraphicsd.lib"
    "${SRC_DIR}/utils/dx_proxy/d3d9.lib"
    "${SRC_DIR}/utils/dx_proxy/d3dx9.lib"
    "${SRC_DIR}/utils/dx_proxy/xgraphics.lib"
)

valve_set_dll_importlib_output(utils_dx_proxy_DX_Proxy_DX_SDK_VER)
valve_publish_dll(utils_dx_proxy_DX_Proxy_DX_SDK_VER "${GAME_BIN_DIR}")

# utils/dx_proxy
# Generated from: dx_proxy_dx9_v30_pc.vpc, dx_proxy_base.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(UTILS_DX_PROXY_DX_PROXY_DX_SDK_VER_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/dx_proxy/dx_proxy.cpp
    ${SRC_DIR}/utils/dx_proxy/filememcache.cpp
    ${SRC_DIR}/common/dx_proxy/dx_proxy.h
    ${SRC_DIR}/utils/dx_proxy/dxincludeimpl.h
    ${SRC_DIR}/utils/dx_proxy/filememcache.h
)

add_library(utils_dx_proxy_DX_Proxy_DX_SDK_VER SHARED ${UTILS_DX_PROXY_DX_PROXY_DX_SDK_VER_SOURCES})

valve_apply_base_settings(utils_dx_proxy_DX_Proxy_DX_SDK_VER)

target_include_directories(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    DX_PROXY_EXPORTS
)

target_link_libraries(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    tier0
    vstdlib
)

valve_set_dll_importlib_output(utils_dx_proxy_DX_Proxy_DX_SDK_VER)
valve_publish_dll(utils_dx_proxy_DX_Proxy_DX_SDK_VER "${GAME_BIN_DIR}")

# utils/dx_proxy
# Generated from: dx_proxy_ps3.vpc, dx_proxy_base.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(UTILS_DX_PROXY_DX_PROXY_DX_SDK_VER_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/utils/dx_proxy/dx_proxy.cpp
    ${SRC_DIR}/utils/dx_proxy/filememcache.cpp
    ${SRC_DIR}/utils/dx_proxy/dx_proxy_ps3.cpp
    ${SRC_DIR}/common/dx_proxy/dx_proxy.h
    ${SRC_DIR}/utils/dx_proxy/dxincludeimpl.h
    ${SRC_DIR}/utils/dx_proxy/filememcache.h
)

add_library(utils_dx_proxy_DX_Proxy_DX_SDK_VER SHARED ${UTILS_DX_PROXY_DX_PROXY_DX_SDK_VER_SOURCES})

valve_apply_base_settings(utils_dx_proxy_DX_Proxy_DX_SDK_VER)

target_include_directories(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/ps3sdk/cell/host-win32/cg/include
    ${SRC_DIR}/ps3sdk/cell/host-common/include
)

target_compile_definitions(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    DX_PROXY_EXPORTS
)

target_link_libraries(utils_dx_proxy_DX_Proxy_DX_SDK_VER PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    tier0
    vstdlib
    "${SRC_DIR}/ps3sdk/cell/host-win32/cg/lib/libcgc.lib"
    "${SRC_DIR}/dx10sdk/Lib/x86/dxguid.lib"
    "${SRC_DIR}/ps3sdk/cell/host-win32/cg/lib/SCEShaderPerf.lib"
)

valve_set_dll_importlib_output(utils_dx_proxy_DX_Proxy_DX_SDK_VER)
valve_publish_dll(utils_dx_proxy_DX_Proxy_DX_SDK_VER "${GAME_BIN_DIR}")
