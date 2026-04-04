# =============================================================================
# soundsystem_lowlevel
# Generated from: soundsystem_lowlevel.vcxproj
# =============================================================================

set(SOUNDSYSTEM_LOWLEVEL_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    device_dsound.cpp
    device_null.cpp
    device_xaudio2.cpp
    mix.cpp
    simple_filter.cpp
    windows_audio.cpp
)

add_library(soundsystem_lowlevel STATIC ${SOUNDSYSTEM_LOWLEVEL_SOURCES})

valve_apply_base_settings(soundsystem_lowlevel)

target_include_directories(soundsystem_lowlevel PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/thirdparty/dxsdk/include
)

target_compile_definitions(soundsystem_lowlevel PRIVATE
    _LIB
    LIBNAME=soundsystem_lowlevel
)

valve_set_static_lib_output(soundsystem_lowlevel)
