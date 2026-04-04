# =============================================================================
# appframework
# Generated from: appframework.vcxproj
# =============================================================================

set(APPFRAMEWORK_SOURCES
    AppSystemGroup.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    matsysapp.cpp
    VguiMatSysApp.cpp
    WinApp.cpp
)

add_library(appframework STATIC ${APPFRAMEWORK_SOURCES})

valve_apply_base_settings(appframework)

target_compile_definitions(appframework PRIVATE
    _LIB
    LIBNAME=appframework
)

valve_set_static_lib_output(appframework)
