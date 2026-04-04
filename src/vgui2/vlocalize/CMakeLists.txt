# =============================================================================
# Vlocalize
# Generated from: vlocalize.vcxproj
# =============================================================================

set(VLOCALIZE_SOURCES
    CreateTokenDialog.cpp
    LocalizationDialog.cpp
    main.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp
)

add_executable(Vlocalize ${VLOCALIZE_SOURCES})

valve_apply_base_settings(Vlocalize)

target_include_directories(Vlocalize PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
)

target_compile_definitions(Vlocalize PRIVATE
    MEMOVERRIDE_MODULE=vlocalize
)

target_link_libraries(Vlocalize PRIVATE
    # 系统库
    WS2_32.LIB
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_publish_exe(Vlocalize "${GAME_DIR}")
