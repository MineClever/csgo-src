# src/togl
# Generated from: togl.vpc, vpc_scripts\source_dll_base.vpc, vpc_scripts\platform_dirs.vpc, vpc_scripts\source_base.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc, vpc_scripts\source_video_base.vpc

set(TOGL_TOGL_SOURCES
    ${SRC_DIR}/togl/dx9asmtogl2.cpp
    ${SRC_DIR}/togl/dxabstract.cpp
    ${SRC_DIR}/togl/glentrypoints.cpp
    ${SRC_DIR}/togl/glmgr.cpp
    ${SRC_DIR}/togl/glmgrbasics.cpp
    ${SRC_DIR}/togl/cglmtex.cpp
    ${SRC_DIR}/togl/cglmfbo.cpp
    ${SRC_DIR}/togl/cglmprogram.cpp
    ${SRC_DIR}/togl/cglmbuffer.cpp
    ${SRC_DIR}/togl/cglmquery.cpp
    ${SRC_DIR}/togl/dx9asmtogl2.h
    ${SRC_DIR}/togl/glmgr_flush.inl
    ${SRC_DIR}/public/togl/rendermechanism.h
    ${SRC_DIR}/public/togl/dxabstract.h
    ${SRC_DIR}/public/togl/dxabstract_types.h
    ${SRC_DIR}/public/togl/glbase.h
    ${SRC_DIR}/public/togl/glentrypoints.h
    ${SRC_DIR}/public/togl/glmgr.h
    ${SRC_DIR}/public/togl/glmdebug.h
    ${SRC_DIR}/public/togl/glmgrbasics.h
    ${SRC_DIR}/public/togl/glmgrext.h
    ${SRC_DIR}/public/togl/glmdisplay.h
    ${SRC_DIR}/public/togl/glmdisplaydb.h
    ${SRC_DIR}/public/togl/glfuncs.h
    ${SRC_DIR}/public/togl/cglmtex.h
    ${SRC_DIR}/public/togl/cglmfbo.h
    ${SRC_DIR}/public/togl/cglmprogram.h
    ${SRC_DIR}/public/togl/cglmbuffer.h
    ${SRC_DIR}/public/togl/cglmquery.h
)

add_library(togl_togl SHARED ${TOGL_TOGL_SOURCES})

valve_apply_base_settings(togl_togl)

target_include_directories(togl_togl PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}
)

target_compile_definitions(togl_togl PRIVATE
    _USRDLL
    GL_GLEXT_PROTOTYPES
    DX_TO_GL_ABSTRACTION
    TOGL_DLL_EXPORT
    PROTECTED_THINGS_ENABLE
    strncpy=use_Q_strncpy_instead
    _snprintf=use_Q_snprintf_instead
)

target_link_libraries(togl_togl PRIVATE
    tier1
    interfaces
    tier2
    mathlib
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
)

set_target_properties(togl_togl PROPERTIES OUTPUT_NAME "togl")

valve_set_dll_importlib_output(togl_togl)
valve_publish_dll(togl_togl "${GAME_BIN_DIR}")
