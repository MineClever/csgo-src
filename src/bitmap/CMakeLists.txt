# =============================================================================
# bitmap
# Generated from: bitmap.vcxproj
# =============================================================================

set(BITMAP_SOURCES
    bitmap.cpp
    colorconversion.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    floatbitmap.cpp
    floatbitmap2.cpp
    floatbitmap3.cpp
    floatbitmap4.cpp
    floatbitmap_bilateralfilter.cpp
    floatcubemap.cpp
    imageformat.cpp
    psd.cpp
    psheet.cpp
    resample.cpp
    texturepacker.cpp
    tgaloader.cpp
    tgawriter.cpp
)

add_library(bitmap STATIC ${BITMAP_SOURCES})

valve_apply_base_settings(bitmap)

target_include_directories(bitmap PRIVATE
    ${SRC_DIR}/dx9sdk/include
)

target_compile_definitions(bitmap PRIVATE
    _LIB
    LIBNAME=bitmap
)

target_link_libraries(bitmap PUBLIC
    prebuilt::nvtc
)

valve_set_static_lib_output(bitmap)

# =============================================================================
# bitmap_byteswap
# Generated from: bitmap_byteswap.vcxproj
# =============================================================================

set(BITMAP_BYTESWAP_SOURCES
    ${SRC_DIR}/common/debug_lib_check.cpp
    ImageByteSwap.cpp
)

add_library(bitmap_byteswap STATIC ${BITMAP_BYTESWAP_SOURCES})

valve_apply_base_settings(bitmap_byteswap)

target_include_directories(bitmap_byteswap PRIVATE
    $(XEDK)/include/win32
)

target_compile_definitions(bitmap_byteswap PRIVATE
    _LIB
    LIBNAME=bitmap_byteswap
)

valve_set_static_lib_output(bitmap_byteswap)
