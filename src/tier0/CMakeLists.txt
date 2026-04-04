# =============================================================================
# tier0
# Generated from: tier0.vcxproj
# =============================================================================

set(TIER0_SOURCES
    assert_dialog.cpp
    commandline.cpp
    cpu.cpp
    cpumonitoring.cpp
    cputopology.cpp
    dbg.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    dynfunction.cpp
    etwprof.cpp
    fasttimer.cpp
    logging.cpp
    mem.cpp
    mem_helpers.cpp
    memdbg.cpp
    memprocessheap.cpp
    memstd.cpp
    memvalidate.cpp
    memvirt.cpp
    minidump.cpp
    miniprofiler.cpp
    perfstats.cpp
    platform.cpp
    platform_independent.cpp
    platwindow.cpp
    pme.cpp
    PMELib.cpp
    progressbar.cpp
    security.cpp
    stackstats.cpp
    stacktools.cpp
    systeminformation.cpp
    threadtools.cpp
    tier0_strtools.cpp
    tslist.cpp
    ${SRC_DIR}/unitlib/unitlib.cpp
    vatoms.cpp
    vcrmode.cpp
    vprof.cpp
    vtuneinterface.cpp
    win32consoleio.cpp
    ${SRC_DIR}/tier0/dlmalloc/malloc.cpp
)

add_library(tier0 SHARED ${TIER0_SOURCES})

valve_apply_base_settings(tier0)

target_compile_definitions(tier0 PRIVATE
    _USRDLL
    DLLNAME=tier0
    MEMOVERRIDE_MODULE=tier0
    TIER0_DLL_EXPORT
    CROSS_PLATFORM_VERSION=1
    THREAD_MUTEX_TRACING_ENABLED
)

target_link_libraries(tier0 PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
    ws2_32.lib
)

valve_set_dll_importlib_output(tier0)

valve_publish_dll(tier0 "${GAME_BIN_DIR}")
