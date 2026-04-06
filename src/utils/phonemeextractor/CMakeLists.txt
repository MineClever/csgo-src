# utils/phonemeextractor
# Generated from: phonemeextractor.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(UTILS_PHONEMEEXTRACTOR_PHONEMEEXTRACTOR_SOURCES
    ${SRC_DIR}/utils/phonemeextractor/extractor_utils.cpp
    ${SRC_DIR}/public/phonemeconverter.cpp
    ${SRC_DIR}/public/sentence.cpp
    ${SRC_DIR}/utils/phonemeextractor/phonemeextractor.cpp
    ${SRC_DIR}/public/tier0/basetypes.h
    ${SRC_DIR}/public/tier0/dbg.h
    ${SRC_DIR}/public/tier0/fasttimer.h
    ${SRC_DIR}/public/appframework/iappsystem.h
    ${SRC_DIR}/public/mathlib/mathlib.h
    ${SRC_DIR}/public/phonemeconverter.h
    ${SRC_DIR}/public/phonemeextractor/phonemeextractor.h
    ${SRC_DIR}/public/tier0/platform.h
    ${SRC_DIR}/public/sentence.h
    ${SRC_DIR}/public/string_t.h
    ${SRC_DIR}/public/tier1/strtools.h
    ${SRC_DIR}/public/tier1/utllinkedlist.h
    ${SRC_DIR}/public/tier1/utlmemory.h
    ${SRC_DIR}/public/tier1/utlvector.h
    ${SRC_DIR}/public/mathlib/vector.h
    ${SRC_DIR}/public/mathlib/vector2d.h
    ${SRC_DIR}/public/vstdlib/vstdlib.h
)

add_library(utils_phonemeextractor_Phonemeextractor SHARED ${UTILS_PHONEMEEXTRACTOR_PHONEMEEXTRACTOR_SOURCES})

valve_apply_base_settings(utils_phonemeextractor_Phonemeextractor)

target_include_directories(utils_phonemeextractor_Phonemeextractor PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/common
    ${SRC_DIR}/utils/hlfaceposer
    ${SRC_DIR}/utils/sapi51/include
)

target_compile_definitions(utils_phonemeextractor_Phonemeextractor PRIVATE
    _USRDLL
    PHONEMEEXTRACTOR_EXPORTS
)

target_link_libraries(utils_phonemeextractor_Phonemeextractor PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    odbc32.lib
    odbccp32.lib
    mathlib
    "${SRC_DIR}/utils/sapi51/lib/i386/sapi.lib"
)

set_target_properties(utils_phonemeextractor_Phonemeextractor PROPERTIES OUTPUT_NAME "Phonemeextractor")

valve_set_dll_importlib_output(utils_phonemeextractor_Phonemeextractor)
valve_publish_dll(utils_phonemeextractor_Phonemeextractor "${GAME_BIN_DIR}")

# utils/phonemeextractor
# Generated from: phonemeextractor_ims.vpc, vpc_scripts\source_dll_win32_base.vpc, vpc_scripts\version.vpc, vpc_scripts\loadaddress.vpc, vpc_scripts\source_dll_win32_debug.vpc, vpc_scripts\source_dll_win32_release.vpc, vpc_scripts\source_win32_base.vpc, vpc_scripts\source_lowest_base.vpc

set(UTILS_PHONEMEEXTRACTOR_PHONEMEEXTRACTOR_IMS_SOURCES
    ${SRC_DIR}/utils/phonemeextractor/extractor_utils.cpp
    ${SRC_DIR}/public/phonemeconverter.cpp
    ${SRC_DIR}/public/sentence.cpp
    ${SRC_DIR}/utils/phonemeextractor/phonemeextractor_ims.cpp
    ${SRC_DIR}/public/tier0/basetypes.h
    ${SRC_DIR}/public/tier0/dbg.h
    ${SRC_DIR}/public/tier0/fasttimer.h
    ${SRC_DIR}/public/appframework/iappsystem.h
    ${SRC_DIR}/public/mathlib/mathlib.h
    ${SRC_DIR}/public/phonemeconverter.h
    ${SRC_DIR}/public/phonemeextractor/phonemeextractor.h
    ${SRC_DIR}/public/tier0/platform.h
    ${SRC_DIR}/public/sentence.h
    ${SRC_DIR}/public/string_t.h
    ${SRC_DIR}/public/tier1/strtools.h
    ${SRC_DIR}/public/tier1/utllinkedlist.h
    ${SRC_DIR}/public/tier1/utlmemory.h
    ${SRC_DIR}/public/tier1/utlvector.h
    ${SRC_DIR}/public/mathlib/vector.h
    ${SRC_DIR}/public/mathlib/vector2d.h
    ${SRC_DIR}/public/vstdlib/vstdlib.h
)

add_library(utils_phonemeextractor_Phonemeextractor_ims SHARED ${UTILS_PHONEMEEXTRACTOR_PHONEMEEXTRACTOR_IMS_SOURCES})

valve_apply_base_settings(utils_phonemeextractor_Phonemeextractor_ims)

target_include_directories(utils_phonemeextractor_Phonemeextractor_ims PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/utils/common
    ${SRC_DIR}/utils/hlfaceposer
    ${SRC_DIR}/utils/sapi51/include
)

target_compile_definitions(utils_phonemeextractor_Phonemeextractor_ims PRIVATE
    _USRDLL
    PHONEMEEXTRACTOR_EXPORTS
)

target_link_libraries(utils_phonemeextractor_Phonemeextractor_ims PRIVATE
    tier1
    interfaces
    shell32.lib
    user32.lib
    advapi32.lib
    gdi32.lib
    comdlg32.lib
    ole32.lib
    odbc32.lib
    odbccp32.lib
    mathlib
    "${SRC_DIR}/utils/sapi51/lib/i386/sapi.lib"
)

set_target_properties(utils_phonemeextractor_Phonemeextractor_ims PROPERTIES OUTPUT_NAME "Phonemeextractor_ims")

valve_set_dll_importlib_output(utils_phonemeextractor_Phonemeextractor_ims)
valve_publish_dll(utils_phonemeextractor_Phonemeextractor_ims "${GAME_BIN_DIR}")
