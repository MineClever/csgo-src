# =============================================================================
# Pet
# Generated from: pet.vcxproj
# =============================================================================

set(PET_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/interpolatortypes.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    particlesystemdefinitionbrowser.cpp
    particlesystempropertiescontainer.cpp
    petdoc.cpp
    pettool.cpp
    ${SRC_DIR}/public/registry.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp
)

add_library(Pet SHARED ${PET_SOURCES})

valve_apply_base_settings(Pet)

target_include_directories(Pet PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/tools/pet
    ${SRC_DIR}/game/shared
)

target_compile_definitions(Pet PRIVATE
    _USRDLL
    DLLNAME=pet
    MEMOVERRIDE_MODULE=pet
    PET_EXPORTS
)

target_link_libraries(Pet PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    vstdlib
    tier3
    Dmxloader
    Datamodel
    Movieobjects
    Dmserializers
    Dme_controls
    bonesetup
    mathlib
    vgui_controls
    matsys_controls
    Toolutils
    prebuilt::dmeutils
    particles
    bitmap
    # 系统库
    Psapi.lib
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
)

valve_set_dll_importlib_output(Pet)

valve_publish_dll(Pet "${GAME_BIN_DIR}")
