# =============================================================================
# tier2
# Generated from: tier2.vcxproj
# =============================================================================

set(TIER2_SOURCES
    beamsegdraw.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    defaultfilesystem.cpp
    fileutils.cpp
    interval.cpp
    keybindings.cpp
    ${SRC_DIR}/public/map_utils.cpp
    ${SRC_DIR}/public/materialsystem/MaterialSystemUtil.cpp
    meshutils.cpp
    p4helpers.cpp
    renderutils.cpp
    resourceprecacher.cpp
    riff.cpp
    schemaclassbinding.cpp
    socketcreator.cpp
    soundutils.cpp
    tier2.cpp
    tier2_logging.cpp
    tokenreader.cpp
    util_init.cpp
    utlstreambuffer.cpp
    vconfig.cpp
)

add_library(tier2 STATIC ${TIER2_SOURCES})

valve_apply_base_settings(tier2)

target_include_directories(tier2 PRIVATE
    ${SRC_DIR}/public/tier2
)

target_compile_definitions(tier2 PRIVATE
    _LIB
    LIBNAME=tier2
)

valve_set_static_lib_output(tier2)
