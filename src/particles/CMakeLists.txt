# =============================================================================
# particles
# Generated from: particles.vcxproj
# =============================================================================

set(PARTICLES_SOURCES
    addbuiltin_ops.cpp
    builtin_constraints.cpp
    builtin_initializers.cpp
    builtin_particle_emitters.cpp
    builtin_particle_forces.cpp
    builtin_particle_ops.cpp
    builtin_particle_render_ops.cpp
    ${SRC_DIR}/common/debug_lib_check.cpp
    particle_snapshot.cpp
    particle_sort.cpp
    particles.cpp
)

add_library(particles STATIC ${PARTICLES_SOURCES})

valve_apply_base_settings(particles)

target_compile_definitions(particles PRIVATE
    _LIB
    LIBNAME=particles
    $EXTRADEFINES
)

valve_set_static_lib_output(particles)
