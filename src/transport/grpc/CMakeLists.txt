# gRPC transport adapters are production-boundary code and only compile when
# generated gRPC stubs are explicitly enabled.

if(NOT SR_ENABLE_GRPC)
    add_library(sr_grpc_transport INTERFACE)
    return()
endif()

add_library(sr_grpc_transport STATIC
    admin_grpc_adapter.cpp
    gateway_grpc_adapter.cpp
    query_grpc_adapter.cpp
)

target_include_directories(sr_grpc_transport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(sr_grpc_transport PUBLIC
    sr_common
    sr_gateway
    sr_query
    signalroute_proto
)
