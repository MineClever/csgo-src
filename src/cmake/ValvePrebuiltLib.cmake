# =============================================================================
# ValvePrebuiltLib.cmake
# 为 lib/public/ 和 lib/common/ 中已有的预构建 .lib 创建 IMPORTED 目标。
# 仅针对我们不在 CMake 中自行构建的库。
# =============================================================================

# -----------------------------------------------------------------------------
# 内部辅助函数
# -----------------------------------------------------------------------------
function(_valve_import_lib TARGET_NAME LIB_PATH)
    if(NOT TARGET ${TARGET_NAME})
        add_library(${TARGET_NAME} STATIC IMPORTED GLOBAL)
        set_target_properties(${TARGET_NAME} PROPERTIES
            IMPORTED_LOCATION "${LIB_PATH}"
        )
    endif()
endfunction()

function(_valve_import_lib_configaware TARGET_NAME DEBUG_PATH RELEASE_PATH)
    if(NOT TARGET ${TARGET_NAME})
        add_library(${TARGET_NAME} STATIC IMPORTED GLOBAL)
        set_target_properties(${TARGET_NAME} PROPERTIES
            IMPORTED_LOCATION_DEBUG   "${DEBUG_PATH}"
            IMPORTED_LOCATION_RELEASE "${RELEASE_PATH}"
            IMPORTED_LOCATION         "${RELEASE_PATH}"  # 默认 fallback
        )
    endif()
endfunction()

# -----------------------------------------------------------------------------
# 注册所有预构建库（在 src/CMakeLists.txt 中调用一次）
# -----------------------------------------------------------------------------
function(valve_setup_prebuilt_libs)

    # --- lib/public/ 中的纯预构建库（非 CMake 构建目标）---

    # Steam
    _valve_import_lib(prebuilt::steam_api "${LIB_PUBLIC}/steam_api.lib")

    # Protobuf（我们不自行构建 protobuf，使用预编译版本）
    _valve_import_lib(prebuilt::libprotobuf "${LIB_PUBLIC}/libprotobuf.lib")

    # zlib
    _valve_import_lib(prebuilt::libz "${LIB_PUBLIC}/libz.lib")

    # nvtristrip
    _valve_import_lib(prebuilt::nvtristrip "${LIB_PUBLIC}/nvtristrip.lib")

    # nvtc（纹理压缩）
    _valve_import_lib(prebuilt::nvtc "${LIB_PUBLIC}/nvtc.lib")

    # vmpi（分布式编译相关）
    _valve_import_lib(prebuilt::vmpi "${LIB_PUBLIC}/vmpi.lib")

    # socketlib
    _valve_import_lib(prebuilt::socketlib "${LIB_PUBLIC}/socketlib.lib")

    # steamdatagramlib
    _valve_import_lib(prebuilt::steamdatagramlib "${LIB_PUBLIC}/steamdatagramlib.lib")

    # gcsdk（Game Coordinator SDK，预构建）
    _valve_import_lib(prebuilt::gcsdk "${LIB_PUBLIC}/gcsdk.lib")
    _valve_import_lib(prebuilt::gcsdk_gc "${LIB_PUBLIC}/gcsdk_gc.lib")

    # CEF（Chromium Embedded Framework）
    _valve_import_lib(prebuilt::libcef "${LIB_PUBLIC}/libcef.lib")

    # dmeutils
    _valve_import_lib(prebuilt::dmeutils "${LIB_PUBLIC}/dmeutils.lib")

    # fbxutils
    _valve_import_lib(prebuilt::fbxutils "${LIB_PUBLIC}/fbxutils.lib")

    # togl（OpenGL 到 DirectX 转换层）
    _valve_import_lib(prebuilt::togl "${LIB_PUBLIC}/togl.lib")

    # ATI 纹理压缩
    _valve_import_lib(prebuilt::ati_compress_mt_vc8 "${LIB_PUBLIC}/ati_compress_mt_vc8.lib")

    # --- lib/common/ 中的第三方预构建库 ---

    _valve_import_lib(prebuilt::Steam   "${LIB_COMMON}/Steam.lib")
    _valve_import_lib(prebuilt::bzip2   "${LIB_COMMON}/bzip2.lib")
    _valve_import_lib(prebuilt::jpeglib "${LIB_COMMON}/jpeglib.lib")
    _valve_import_lib(prebuilt::libjpeg "${LIB_COMMON}/libjpeg.lib")
    _valve_import_lib(prebuilt::libpng  "${LIB_COMMON}/libpng.lib")
    _valve_import_lib(prebuilt::mss32   "${LIB_COMMON}/mss32.lib")

    # Havok 物理库
    _valve_import_lib(prebuilt::havana_constraints "${LIB_COMMON}/havana_constraints.lib")
    _valve_import_lib(prebuilt::hk_base            "${LIB_COMMON}/hk_base.lib")
    _valve_import_lib(prebuilt::hk_math            "${LIB_COMMON}/hk_math.lib")
    _valve_import_lib(prebuilt::ivp_compactbuilder  "${LIB_COMMON}/ivp_compactbuilder.lib")
    _valve_import_lib(prebuilt::ivp_physics        "${LIB_COMMON}/ivp_physics.lib")

    # --- lib/win32/2015/ debug/release 各异的库 ---

    _valve_import_lib_configaware(prebuilt::cryptlib
        "${SRC_DIR}/lib/win32/2015/debug/cryptlib.lib"
        "${SRC_DIR}/lib/win32/2015/release/cryptlib.lib"
    )

endfunction()

# -----------------------------------------------------------------------------
# 便利宏：将常见系统库组合成一个 INTERFACE 目标
# 大多数 DLL/EXE 目标都需要这些
# -----------------------------------------------------------------------------
function(valve_setup_system_libs)
    if(NOT TARGET valve::system_libs)
        add_library(valve::system_libs INTERFACE IMPORTED GLOBAL)
        target_link_libraries(valve::system_libs INTERFACE
            shell32.lib
            user32.lib
            advapi32.lib
            gdi32.lib
            comdlg32.lib
            ole32.lib
            ws2_32.lib
        )
    endif()
endfunction()
