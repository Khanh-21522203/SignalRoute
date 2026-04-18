# Proto library build configuration
# Generates C++ sources from .proto files using protoc + grpc_cpp_plugin.

# TODO: Find protobuf and gRPC packages
# find_package(Protobuf REQUIRED)
# find_package(gRPC REQUIRED)

# Collect all .proto files
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/signalroute/*.proto")

# TODO: Generate C++ sources from proto files
# protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
# grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_FILES})

# TODO: Create a library target for generated proto sources
# add_library(signalroute_proto
#     ${PROTO_SRCS}
#     ${PROTO_HDRS}
#     ${GRPC_SRCS}
#     ${GRPC_HDRS}
# )
# target_link_libraries(signalroute_proto PUBLIC
#     protobuf::libprotobuf
#     gRPC::grpc++
# )
# target_include_directories(signalroute_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
