cmake_minimum_required(VERSION 3.20)

option(VALVE_CONFIGURE_VPC_ONLY "Configure only the VPC toolchain targets." OFF)

if(VALVE_CONFIGURE_VPC_ONLY)
    project(VPC CXX C ASM_MASM)
else()
    project(CSGO CXX C ASM_MASM)
endif()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
include("${REPO_ROOT}/src/cmake/ValveProjectSetup.cmake")

if(VALVE_CONFIGURE_VPC_ONLY)
    include("${REPO_ROOT}/src/cmake/ValveVPCProject.cmake")
    valve_configure_vpc_project()
else()
    add_subdirectory(src)
endif()
