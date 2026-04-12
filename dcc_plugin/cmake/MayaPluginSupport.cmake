function(maya_dmx_configure_sdk)
    if(NOT EXISTS "${MAYA_INSTALL_ROOT}")
        message(FATAL_ERROR "Maya install root not found: ${MAYA_INSTALL_ROOT}")
    endif()

    if(NOT EXISTS "${MAYA_DEVKIT_ROOT}")
        message(WARNING "Maya DevKit root not found: ${MAYA_DEVKIT_ROOT}")
    endif()

    set(MAYA_DMX_INCLUDE_DIR "${MAYA_INSTALL_ROOT}/include" CACHE PATH "Maya include directory" FORCE)
    set(MAYA_DMX_LIBRARY_DIR "${MAYA_INSTALL_ROOT}/lib" CACHE PATH "Maya library directory" FORCE)

    foreach(_maya_required_dir "${MAYA_DMX_INCLUDE_DIR}" "${MAYA_DMX_LIBRARY_DIR}")
        if(NOT EXISTS "${_maya_required_dir}")
            message(FATAL_ERROR "Required Maya path not found: ${_maya_required_dir}")
        endif()
    endforeach()

    foreach(_maya_lib_name Foundation OpenMaya OpenMayaAnim OpenMayaRender OpenMayaUI)
        find_library(MAYA_DMX_${_maya_lib_name}_LIB
            NAMES ${_maya_lib_name}
            PATHS "${MAYA_DMX_LIBRARY_DIR}"
            NO_DEFAULT_PATH
            REQUIRED
        )
        mark_as_advanced(MAYA_DMX_${_maya_lib_name}_LIB)
    endforeach()

    set(MAYA_DMX_LIBRARIES
        "${MAYA_DMX_Foundation_LIB}"
        "${MAYA_DMX_OpenMaya_LIB}"
        "${MAYA_DMX_OpenMayaAnim_LIB}"
        "${MAYA_DMX_OpenMayaRender_LIB}"
        "${MAYA_DMX_OpenMayaUI_LIB}"
        CACHE INTERNAL "Maya libraries for DMX plugin"
    )
endfunction()

function(maya_dmx_apply_common_target_settings target_name)
    target_compile_features(${target_name} PUBLIC cxx_std_17)
    target_include_directories(${target_name}
        PUBLIC
            "${MAYA_DMX_INCLUDE_DIR}"
            "${DCC_PLUGIN_ROOT}/src"
            "${DCC_PLUGIN_ROOT}/include"
            "${DCC_PLUGIN_ROOT}"
    )
    target_compile_definitions(${target_name}
        PUBLIC
            NT_PLUGIN
            REQUIRE_IOSTREAM
            _BOOL
            NOMINMAX
            _CRT_SECURE_NO_WARNINGS
    )
    target_compile_options(${target_name}
        PRIVATE
            /Zc:__cplusplus
            /permissive-
            /EHsc
            /W4
            /wd4100
            /wd4127
            /wd4251
            /wd4996
    )
endfunction()

function(maya_dmx_configure_plugin_target target_name output_name)
    maya_dmx_apply_common_target_settings(${target_name})
    target_link_libraries(${target_name} PRIVATE ${MAYA_DMX_LIBRARIES})
    if(MAYA_DMX_BUILD_PDB)
        set_target_properties(${target_name} PROPERTIES
            PREFIX ""
            SUFFIX ".mll"
            OUTPUT_NAME "${output_name}"
            ARCHIVE_OUTPUT_DIRECTORY "${MAYA_DMX_OUTPUT_DIR}/$<CONFIG>"
            LIBRARY_OUTPUT_DIRECTORY "${MAYA_DMX_OUTPUT_DIR}/$<CONFIG>"
            RUNTIME_OUTPUT_DIRECTORY "${MAYA_DMX_OUTPUT_DIR}/$<CONFIG>"
            PDB_OUTPUT_DIRECTORY "${MAYA_DMX_OUTPUT_DIR}/$<CONFIG>"
            COMPILE_PDB_OUTPUT_DIRECTORY "${MAYA_DMX_OUTPUT_DIR}/$<CONFIG>"
        )
        if(MSVC)
            set_property(TARGET ${target_name} PROPERTY MSVC_DEBUG_INFORMATION_FORMAT
                "$<$<CONFIG:Debug,RelWithDebInfo,Release,MinSizeRel>:ProgramDatabase>"
            )
            set_property(TARGET ${target_name} APPEND_STRING PROPERTY LINK_FLAGS_RELEASE " /DEBUG:FULL")
            set_property(TARGET ${target_name} APPEND_STRING PROPERTY LINK_FLAGS_MINSIZEREL " /DEBUG:FULL")
        endif()
    else()
        set_target_properties(${target_name} PROPERTIES
            PREFIX ""
            SUFFIX ".mll"
            OUTPUT_NAME "${output_name}"
            ARCHIVE_OUTPUT_DIRECTORY "${MAYA_DMX_OUTPUT_DIR}/$<CONFIG>"
            LIBRARY_OUTPUT_DIRECTORY "${MAYA_DMX_OUTPUT_DIR}/$<CONFIG>"
            RUNTIME_OUTPUT_DIRECTORY "${MAYA_DMX_OUTPUT_DIR}/$<CONFIG>"
        )
        if(MSVC)
            set_property(TARGET ${target_name} APPEND_STRING PROPERTY LINK_FLAGS_RELEASE " /DEBUG:NONE")
            set_property(TARGET ${target_name} APPEND_STRING PROPERTY LINK_FLAGS_MINSIZEREL " /DEBUG:NONE")
        endif()
    endif()
endfunction()
