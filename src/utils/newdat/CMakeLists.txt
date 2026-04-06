# =============================================================================
# Newdat
# Generated from: newdat.vcxproj
# =============================================================================

set(NEWDAT_SOURCES
    ${SRC_DIR}/public/tier0/memoverride.cpp
    newdat.cpp
)

add_executable(Newdat ${NEWDAT_SOURCES})

valve_apply_base_settings(Newdat)

target_include_directories(Newdat PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(Newdat PRIVATE
    MEMOVERRIDE_MODULE=newdat
    PROTECTED_THINGS_DISABLE
)

target_link_libraries(Newdat PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    vstdlib
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_publish_exe(Newdat "${GAME_BIN_DIR}")
