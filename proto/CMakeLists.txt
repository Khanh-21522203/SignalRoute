# Proto library build configuration.
#
# Fallback builds always expose a signalroute_proto target so service libraries
# can link a stable boundary before generated protobuf/gRPC code is enabled.

file(GLOB PROTO_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/signalroute/*.proto")

if(NOT SR_ENABLE_PROTOBUF_GRPC)
    add_library(signalroute_proto INTERFACE)
    target_compile_definitions(signalroute_proto INTERFACE SIGNALROUTE_HAS_PROTOBUF_GRPC=0)
    target_include_directories(signalroute_proto INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
    return()
endif()

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(GENERATED_PROTO_SRCS)
set(GENERATED_PROTO_HDRS)
set(GENERATED_GRPC_SRCS)
set(GENERATED_GRPC_HDRS)

foreach(proto_file ${PROTO_FILES})
    file(RELATIVE_PATH proto_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${proto_file}")
    get_filename_component(proto_dir "${proto_rel}" DIRECTORY)
    get_filename_component(proto_name "${proto_file}" NAME_WE)

    set(proto_src "${CMAKE_CURRENT_BINARY_DIR}/${proto_dir}/${proto_name}.pb.cc")
    set(proto_hdr "${CMAKE_CURRENT_BINARY_DIR}/${proto_dir}/${proto_name}.pb.h")
    set(grpc_src "${CMAKE_CURRENT_BINARY_DIR}/${proto_dir}/${proto_name}.grpc.pb.cc")
    set(grpc_hdr "${CMAKE_CURRENT_BINARY_DIR}/${proto_dir}/${proto_name}.grpc.pb.h")

    add_custom_command(
        OUTPUT "${proto_src}" "${proto_hdr}" "${grpc_src}" "${grpc_hdr}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${proto_dir}"
        COMMAND protobuf::protoc
            --proto_path="${CMAKE_CURRENT_SOURCE_DIR}"
            --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"
            --grpc_out="${CMAKE_CURRENT_BINARY_DIR}"
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
            "${proto_file}"
        DEPENDS "${proto_file}"
        VERBATIM
    )

    list(APPEND GENERATED_PROTO_SRCS "${proto_src}")
    list(APPEND GENERATED_PROTO_HDRS "${proto_hdr}")
    list(APPEND GENERATED_GRPC_SRCS "${grpc_src}")
    list(APPEND GENERATED_GRPC_HDRS "${grpc_hdr}")
endforeach()

add_library(signalroute_proto STATIC
    ${GENERATED_PROTO_SRCS}
    ${GENERATED_PROTO_HDRS}
    ${GENERATED_GRPC_SRCS}
    ${GENERATED_GRPC_HDRS}
)

target_compile_definitions(signalroute_proto PUBLIC SIGNALROUTE_HAS_PROTOBUF_GRPC=1)
target_include_directories(signalroute_proto PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(signalroute_proto PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
)
