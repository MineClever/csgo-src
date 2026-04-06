add_library(maya_dmx_workflow STATIC
    MayaDmxWorkflow.cpp
    MayaDmxWorkflow.h
    MayaDmxWorkflowCommand.cpp
    MayaDmxWorkflowCommand.h
)

target_link_libraries(maya_dmx_workflow PUBLIC maya_dmx_common)
maya_dmx_apply_common_target_settings(maya_dmx_workflow)
