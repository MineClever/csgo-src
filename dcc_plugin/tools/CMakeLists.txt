add_executable(maya_dmx_sample_tool
    DmxSampleTool.cpp
)

target_link_libraries(maya_dmx_sample_tool PRIVATE maya_dmx_serialization)
maya_dmx_apply_common_target_settings(maya_dmx_sample_tool)

set_target_properties(maya_dmx_sample_tool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${DCC_PLUGIN_ROOT}/bin/$<CONFIG>"
)

set(MAYA_DMX_SAMPLE_NAMES
    simple_hierarchy
    simple_mesh
    simple_skinned_mesh
    complex_chr_mesh
)

set(MAYA_DMX_SAMPLE_REGRESSION_OUTPUTS)
set(MAYA_DMX_SAMPLE_REGRESSION_COMMANDS)
foreach(_sample_name IN LISTS MAYA_DMX_SAMPLE_NAMES)
    list(APPEND MAYA_DMX_SAMPLE_REGRESSION_OUTPUTS
        "${CMAKE_BINARY_DIR}/sample_regression/$<CONFIG>/${_sample_name}.dmxb"
        "${CMAKE_BINARY_DIR}/sample_regression/$<CONFIG>/${_sample_name}.roundtrip.dmx"
    )
    list(APPEND MAYA_DMX_SAMPLE_REGRESSION_COMMANDS
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/sample_regression/$<CONFIG>"
        COMMAND "$<TARGET_FILE:maya_dmx_sample_tool>"
            "${DCC_PLUGIN_ROOT}/samples/${_sample_name}.dmx"
            "${CMAKE_BINARY_DIR}/sample_regression/$<CONFIG>/${_sample_name}.dmxb"
        COMMAND "$<TARGET_FILE:maya_dmx_sample_tool>"
            "${CMAKE_BINARY_DIR}/sample_regression/$<CONFIG>/${_sample_name}.dmxb"
            "${CMAKE_BINARY_DIR}/sample_regression/$<CONFIG>/${_sample_name}.roundtrip.dmx"
    )
endforeach()

add_custom_target(maya_dmx_sample_regression
    DEPENDS maya_dmx_sample_tool
    ${MAYA_DMX_SAMPLE_REGRESSION_COMMANDS}
    BYPRODUCTS ${MAYA_DMX_SAMPLE_REGRESSION_OUTPUTS}
    COMMENT "Running Maya DMX sample regression conversions"
)
