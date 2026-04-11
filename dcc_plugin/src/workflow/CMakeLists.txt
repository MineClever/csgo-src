add_library(maya_dmx_workflow STATIC
    BatchManifestStore.cpp
    BatchManifestStore.h
    MayaDmxWorkflow.cpp
    MayaDmxWorkflow.h
    MayaDmxWorkflowCommand.cpp
    MayaDmxWorkflowCommand.h
    WorkflowExecutor.cpp
    WorkflowExecutor.h
    WorkflowPresetStore.cpp
    WorkflowPresetStore.h
    WorkflowSupport.cpp
    WorkflowSupport.h
)

target_link_libraries(maya_dmx_workflow PUBLIC maya_dmx_common)
maya_dmx_apply_common_target_settings(maya_dmx_workflow)
