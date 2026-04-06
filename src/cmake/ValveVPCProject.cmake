function(valve_configure_vpc_project)
    list(APPEND CMAKE_MODULE_PATH "${REPO_ROOT}/src/cmake")
    include(ValveBase)
    include(ValvePostBuild)
    include(ValvePrebuiltLib)

    valve_setup_prebuilt_libs()
    valve_setup_system_libs()

    if(NOT TARGET vstdlib)
        add_library(vstdlib SHARED IMPORTED GLOBAL)
        if(EXISTS "${VALVE_BUILT_LIB_PUBLIC}/vstdlib.lib")
            set_target_properties(vstdlib PROPERTIES
                IMPORTED_IMPLIB "${VALVE_BUILT_LIB_PUBLIC}/vstdlib.lib"
            )
        else()
            message(FATAL_ERROR "Expected prebuilt vstdlib.lib at: ${VALVE_BUILT_LIB_PUBLIC}/vstdlib.lib")
        endif()
    endif()

    add_subdirectory("${REPO_ROOT}/src/interfaces" "${CMAKE_CURRENT_BINARY_DIR}/interfaces")
    add_subdirectory("${REPO_ROOT}/src/tier0" "${CMAKE_CURRENT_BINARY_DIR}/tier0")
    add_subdirectory("${REPO_ROOT}/src/tier1" "${CMAKE_CURRENT_BINARY_DIR}/tier1")
    add_subdirectory("${REPO_ROOT}/src/utils/vpccrccheck" "${CMAKE_CURRENT_BINARY_DIR}/utils_vpccrccheck")
    add_subdirectory("${REPO_ROOT}/src/utils/vpc" "${CMAKE_CURRENT_BINARY_DIR}/utils_vpc")

    if(NOT TARGET vpc_all)
        add_custom_target(vpc_all)
    endif()
    add_dependencies(vpc_all utils_vpc_vpc utils_vpccrccheck_vpccrccheck)

    set_property(DIRECTORY "${REPO_ROOT}" PROPERTY VS_STARTUP_PROJECT vpc_all)
endfunction()
