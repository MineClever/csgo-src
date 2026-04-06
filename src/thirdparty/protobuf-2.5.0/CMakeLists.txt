# =============================================================================
# protobuf 2.5.0
# Built from bundled source instead of linking a prebuilt libprotobuf.lib.
# =============================================================================

set(PROTOBUF_2_5_0_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/io/coded_stream.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/stubs/common.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/descriptor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/descriptor.pb.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/descriptor_database.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/dynamic_message.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/extension_set.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/extension_set_heavy.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/generated_message_reflection.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/generated_message_util.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/io/gzip_stream.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/compiler/importer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/message.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/message_lite.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/stubs/once.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/stubs/atomicops_internals_x86_msvc.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/compiler/parser.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/io/printer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/reflection_ops.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/repeated_field.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/service.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/stubs/structurally_valid.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/stubs/strutil.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/stubs/substitute.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/text_format.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/io/tokenizer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/unknown_field_set.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/wire_format.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/wire_format_lite.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/io/zero_copy_stream.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/io/zero_copy_stream_impl.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/io/zero_copy_stream_impl_lite.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/google/protobuf/stubs/stringprintf.cc
)

add_library(valve_libprotobuf STATIC ${PROTOBUF_2_5_0_SOURCES})
add_library(prebuilt::libprotobuf ALIAS valve_libprotobuf)

valve_apply_base_settings(valve_libprotobuf)

target_include_directories(valve_libprotobuf PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vsprojects
)

target_compile_definitions(valve_libprotobuf PRIVATE
    _LIB
    LIBPROTOBUF_EXPORTS
)

target_compile_options(valve_libprotobuf PRIVATE
    /wd4018
    /wd4146
    /wd4251
    /wd4267
    /wd4305
    /wd4355
    /wd4800
)

set_target_properties(valve_libprotobuf PROPERTIES
    OUTPUT_NAME libprotobuf
)

valve_set_static_lib_output(valve_libprotobuf)
