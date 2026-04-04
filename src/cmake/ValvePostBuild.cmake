# =============================================================================
# ValvePostBuild.cmake
# 等价于 vcxproj PostBuildEvent（构建后复制 DLL/EXE/PDB 到 game/ 目录）
# =============================================================================

# -----------------------------------------------------------------------------
# 函数：valve_publish_dll(TARGET DEST_DIR)
# 构建完成后将 DLL + PDB 复制到目标目录
# 等价于 vcxproj 的 "copy $(TargetFileName) to game/bin/"
# -----------------------------------------------------------------------------
function(valve_publish_dll TARGET DEST_DIR)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEST_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${TARGET}>"
            "${DEST_DIR}/$<TARGET_FILE_NAME:${TARGET}>"
        COMMENT "Publishing ${TARGET} -> ${DEST_DIR}"
        VERBATIM
    )
endfunction()

# -----------------------------------------------------------------------------
# 函数：valve_publish_exe(TARGET DEST_DIR)
# 构建完成后将 EXE 复制到目标目录
# -----------------------------------------------------------------------------
function(valve_publish_exe TARGET DEST_DIR)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEST_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${TARGET}>"
            "${DEST_DIR}/$<TARGET_FILE_NAME:${TARGET}>"
        COMMENT "Publishing ${TARGET} -> ${DEST_DIR}"
        VERBATIM
    )
endfunction()

# -----------------------------------------------------------------------------
# 函数：valve_set_static_lib_output(TARGET)
# 将静态库 .lib 直接输出到 lib/public/（所有配置统一目录）
# 等价于 vcxproj OutputFile = ..\lib\public\xxx.lib
# -----------------------------------------------------------------------------
function(valve_set_static_lib_output TARGET)
    set_target_properties(${TARGET} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY         "${LIB_PUBLIC}"
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${LIB_PUBLIC}"
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${LIB_PUBLIC}"
    )
endfunction()

# -----------------------------------------------------------------------------
# 函数：valve_set_dll_importlib_output(TARGET)
# 将 DLL 的 import lib (.lib) 输出到 lib/public/
# DLL 本身由 valve_publish_dll 负责复制到 game/bin/
# -----------------------------------------------------------------------------
function(valve_set_dll_importlib_output TARGET)
    set_target_properties(${TARGET} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY         "${LIB_PUBLIC}"
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${LIB_PUBLIC}"
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${LIB_PUBLIC}"
    )
endfunction()
