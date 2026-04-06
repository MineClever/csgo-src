# =============================================================================
# vstdlib
# Generated from: vstdlib.vcxproj
# =============================================================================

set(VSTDLIB_SOURCES
    coroutine.cpp
    cvar.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    eventsystem.cpp
    jobthread.cpp
    keyvaluessystem.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    osversion.cpp
    processutils.cpp
    random.cpp
    vcover.cpp
    vstdlib_strtools.cpp
    vstrtools.cpp
)

add_library(vstdlib SHARED ${VSTDLIB_SOURCES})

if(VALVE_TARGET_ARCH STREQUAL "x64")
    set(VSTDLIB_GETSTACKPTR64_OBJ "${CMAKE_CURRENT_BINARY_DIR}/getstackptr64.obj")
    add_custom_command(
        OUTPUT "${VSTDLIB_GETSTACKPTR64_OBJ}"
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /nologo /c /Fo"${VSTDLIB_GETSTACKPTR64_OBJ}" "${CMAKE_CURRENT_SOURCE_DIR}/getstackptr64.masm"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/getstackptr64.masm"
        VERBATIM
        COMMENT "Compiling getstackptr64.masm"
    )
    set_source_files_properties("${VSTDLIB_GETSTACKPTR64_OBJ}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
    target_sources(vstdlib PRIVATE "${VSTDLIB_GETSTACKPTR64_OBJ}")
endif()

valve_apply_base_settings(vstdlib)

target_compile_definitions(vstdlib PRIVATE
    _USRDLL
    DLLNAME=vstdlib
    MEMOVERRIDE_MODULE=vstdlib
    VSTDLIB_DLL_EXPORT
)

target_link_libraries(vstdlib PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(vstdlib)

valve_publish_dll(vstdlib "${GAME_BIN_DIR}")
