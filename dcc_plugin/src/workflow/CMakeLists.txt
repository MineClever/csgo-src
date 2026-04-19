add_library(maya_workflow_dmx STATIC
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

target_link_libraries(maya_workflow_dmx PUBLIC maya_common_dmx)
maya_dmx_apply_common_target_settings(maya_workflow_dmx)
