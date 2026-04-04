# =============================================================================
# Matchmaking (CSGO)
# Generated from: matchmaking_csgo.vcxproj
# =============================================================================

set(MATCHMAKING_CSGO_SOURCES
    ${SRC_DIR}/matchmaking/cstrike15/mm_title.cpp
    ${SRC_DIR}/matchmaking/cstrike15/mm_title_gamesettingsmgr.cpp
    ${SRC_DIR}/matchmaking/cstrike15/mm_title_main.cpp
    ${SRC_DIR}/matchmaking/cstrike15/mm_title_richpresence.cpp
    ${SRC_DIR}/matchmaking/cstrike15/mm_title_titledata.cpp
    ${SRC_DIR}/game/shared/cstrike15/gametypes.cpp
    ${SRC_DIR}/common/platforminputdevice.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
)

add_library(matchmaking_csgo SHARED ${MATCHMAKING_CSGO_SOURCES})

valve_apply_base_settings(matchmaking_csgo)

target_include_directories(matchmaking_csgo PRIVATE
    ${SRC_DIR}/gcsdk/steamextra
    ${SRC_DIR}/thirdparty/protobuf-2.5.0/src
    ${SRC_DIR}/common/xlast_csgo
)

target_compile_definitions(matchmaking_csgo PRIVATE
    _USRDLL
    DLLNAME=matchmaking
    MEMOVERRIDE_MODULE=matchmaking
    NO_STRING_T
    VECTOR
    VERSION_SAFE_STEAM_API_INTERFACES
    PROTECTED_THINGS_ENABLE
    NO_STEAM_GAMECOORDINATOR
    strncpy=use_Q_strncpy_instead
    _snprintf=use_Q_snprintf_instead
    MATCHMAKING_DLL
)

target_link_libraries(matchmaking_csgo PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
    wininet.lib
    ws2_32.lib
    wsock32.lib
)

valve_set_dll_importlib_output(matchmaking_csgo)

valve_publish_dll(matchmaking_csgo "${GAME_BIN_DIR}")

# =============================================================================
# Matchmaking_DS (CSGO)
# Generated from: matchmaking_ds_csgo.vcxproj
# =============================================================================

set(MATCHMAKING_DS_CSGO_SOURCES
    ${SRC_DIR}/matchmaking/cstrike15/mm_title.cpp
    ${SRC_DIR}/matchmaking/cstrike15/mm_title_gamesettingsmgr.cpp
    ${SRC_DIR}/matchmaking/cstrike15/mm_title_main.cpp
    ${SRC_DIR}/matchmaking/cstrike15/mm_title_richpresence.cpp
    ${SRC_DIR}/matchmaking/cstrike15/mm_title_titledata.cpp
    ${SRC_DIR}/game/shared/cstrike15/gametypes.cpp
    ${SRC_DIR}/common/platforminputdevice.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
)

add_library(matchmaking_ds_csgo SHARED ${MATCHMAKING_DS_CSGO_SOURCES})

valve_apply_base_settings(matchmaking_ds_csgo)

target_include_directories(matchmaking_ds_csgo PRIVATE
    ${SRC_DIR}/gcsdk/steamextra
    ${SRC_DIR}/thirdparty/protobuf-2.5.0/src
    ${SRC_DIR}/common/xlast_csgo
)

target_compile_definitions(matchmaking_ds_csgo PRIVATE
    _USRDLL
    DLLNAME=matchmaking_ds
    MEMOVERRIDE_MODULE=matchmaking_ds
    NO_STRING_T
    VECTOR
    VERSION_SAFE_STEAM_API_INTERFACES
    PROTECTED_THINGS_ENABLE
    NO_STEAM_GAMECOORDINATOR
    strncpy=use_Q_strncpy_instead
    _snprintf=use_Q_snprintf_instead
    MATCHMAKING_DS_DLL
    SWDS
)

target_link_libraries(matchmaking_ds_csgo PRIVATE
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
    wininet.lib
    ws2_32.lib
    wsock32.lib
)

valve_set_dll_importlib_output(matchmaking_ds_csgo)

valve_publish_dll(matchmaking_ds_csgo "${GAME_BIN_DIR}")

# =============================================================================
# matchmakingbase
# Generated from: matchmakingbase.vcxproj
# =============================================================================

set(MATCHMAKINGBASE_SOURCES
    matchmakingqos.cpp
    mm_events.cpp
    mm_extensions.cpp
    mm_framework.cpp
    mm_netmsgcontroller.cpp
    mm_session.cpp
    mm_voice.cpp
    steam_apihook.cpp
    steam_datacenterjobs.cpp
    steam_lobbyapi.cpp
    x360_xlsp_cmd.cpp
    ds_searcher.cpp
    match_searcher.cpp
    mm_netmgr.cpp
    mm_session_offline_custom.cpp
    mm_session_online_client.cpp
    mm_session_online_host.cpp
    mm_session_online_search.cpp
    mm_session_online_teamsearch.cpp
    sys_session.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    main.cpp
    datacenter.cpp
    leaderboards.cpp
    matchsystem.cpp
    mm_dlc.cpp
    player.cpp
    playermanager.cpp
    playerrankingdata.cpp
    searchmanager.cpp
    servermanager.cpp
    extkeyvalues.cpp
)

add_library(matchmakingbase STATIC ${MATCHMAKINGBASE_SOURCES})

valve_apply_base_settings(matchmakingbase)

target_include_directories(matchmakingbase PRIVATE
    ${SRC_DIR}/gcsdk/steamextra
    ${SRC_DIR}/game/shared
    ${SRC_DIR}/thirdparty/protobuf-2.5.0/src
)

target_compile_definitions(matchmakingbase PRIVATE
    _LIB
    LIBNAME=matchmakingbase
    NO_STRING_T
    VECTOR
    VERSION_SAFE_STEAM_API_INTERFACES
    PROTECTED_THINGS_ENABLE
    NO_STEAM_GAMECOORDINATOR
    strncpy=use_Q_strncpy_instead
    _snprintf=use_Q_snprintf_instead
    MATCHMAKING_DLL
)

valve_set_static_lib_output(matchmakingbase)

# =============================================================================
# matchmakingbase_ds
# Generated from: matchmakingbase_ds.vcxproj
# =============================================================================

set(MATCHMAKINGBASE_DS_SOURCES
    matchmakingqos.cpp
    mm_events.cpp
    mm_extensions.cpp
    mm_framework.cpp
    mm_netmsgcontroller.cpp
    mm_voice.cpp
    steam_apihook.cpp
    steam_datacenterjobs.cpp
    steam_lobbyapi.cpp
    x360_xlsp_cmd.cpp
    mm_netmgr.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    main.cpp
    datacenter.cpp
    matchsystem.cpp
    mm_dlc.cpp
    playermanager.cpp
    playerrankingdata.cpp
    servermanager.cpp
    extkeyvalues.cpp
)

add_library(matchmakingbase_ds STATIC ${MATCHMAKINGBASE_DS_SOURCES})

valve_apply_base_settings(matchmakingbase_ds)

target_include_directories(matchmakingbase_ds PRIVATE
    ${SRC_DIR}/gcsdk/steamextra
    ${SRC_DIR}/game/shared
    ${SRC_DIR}/thirdparty/protobuf-2.5.0/src
)

target_compile_definitions(matchmakingbase_ds PRIVATE
    _LIB
    LIBNAME=matchmakingbase_ds
    NO_STRING_T
    VECTOR
    VERSION_SAFE_STEAM_API_INTERFACES
    PROTECTED_THINGS_ENABLE
    NO_STEAM_GAMECOORDINATOR
    strncpy=use_Q_strncpy_instead
    _snprintf=use_Q_snprintf_instead
    MATCHMAKING_DS_DLL
    SWDS
)

valve_set_static_lib_output(matchmakingbase_ds)
