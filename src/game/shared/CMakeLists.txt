# game/shared
#
# This directory contains legacy VPC/VGC definitions but does not yet
# have a full CMake port. This placeholder keeps repository-level
# CMake coverage aligned with the original VPC layout.

# VPC sources in this directory:
# - base_gcmessages_include.vpc
# - schema_cstrike.vpc
# - schema_cstrike15.vpc

# Subdirectories that already provide CMake entrypoints:
add_subdirectory(cstrike15)
