# Central dependency discovery for SignalRoute production adapters.
# This file intentionally does not rewrite adapter behavior. It only exposes
# compile definitions and link targets when an adapter is explicitly enabled.

add_library(sr_dependencies INTERFACE)

target_compile_definitions(sr_dependencies INTERFACE
    SIGNALROUTE_FALLBACK_RUNTIME=1
    SIGNALROUTE_HAS_PROTOBUF_GRPC=$<BOOL:${SR_ENABLE_PROTOBUF_GRPC}>
    SIGNALROUTE_HAS_H3=$<BOOL:${SR_ENABLE_REAL_H3}>
    SIGNALROUTE_HAS_REDIS=$<BOOL:${SR_ENABLE_REAL_REDIS}>
    SIGNALROUTE_HAS_POSTGIS=$<BOOL:${SR_ENABLE_REAL_POSTGIS}>
    SIGNALROUTE_HAS_KAFKA=$<BOOL:${SR_ENABLE_REAL_KAFKA}>
    SIGNALROUTE_HAS_PROMETHEUS=$<BOOL:${SR_ENABLE_PROMETHEUS}>
    SIGNALROUTE_HAS_TOMLPLUSPLUS=$<BOOL:${SR_ENABLE_TOMLPLUSPLUS}>
)

message(STATUS "SignalRoute dependency provider: ${SR_DEPENDENCY_PROVIDER}")
message(STATUS "SignalRoute fallback runtime: enabled by default")

function(sr_link_one_of description)
    foreach(candidate ${ARGN})
        if(TARGET ${candidate})
            target_link_libraries(sr_dependencies INTERFACE ${candidate})
            message(STATUS "SignalRoute ${description}: using target ${candidate}")
            return()
        endif()
    endforeach()

    string(REPLACE ";" ", " candidate_list "${ARGN}")
    message(FATAL_ERROR "SignalRoute ${description} was requested, but none of these CMake targets exist after find_package: ${candidate_list}")
endfunction()

if(SR_ENABLE_REAL_H3)
    find_package(h3 CONFIG REQUIRED)
    sr_link_one_of("H3" h3::h3 h3)
endif()

if(SR_ENABLE_REAL_REDIS)
    find_package(hiredis CONFIG REQUIRED)
    find_package(redis++ CONFIG REQUIRED)
    sr_link_one_of("hiredis" hiredis::hiredis hiredis)
    sr_link_one_of("redis++" redis++::redis++ redis++::redis++_static redis++)
endif()

if(SR_ENABLE_REAL_POSTGIS)
    find_package(PostgreSQL REQUIRED)
    sr_link_one_of("PostgreSQL/libpq" PostgreSQL::PostgreSQL)
endif()

if(SR_ENABLE_REAL_KAFKA)
    find_package(RdKafka CONFIG REQUIRED)
    sr_link_one_of("librdkafka" RdKafka::rdkafka++ RdKafka::rdkafka rdkafka++ rdkafka)
endif()

if(SR_ENABLE_PROMETHEUS)
    find_package(prometheus-cpp CONFIG REQUIRED)
    sr_link_one_of("prometheus-cpp" prometheus-cpp::core prometheus-cpp::pull)
endif()

if(SR_ENABLE_TOMLPLUSPLUS)
    find_package(tomlplusplus CONFIG REQUIRED)
    sr_link_one_of("toml++" tomlplusplus::tomlplusplus tomlplusplus_tomlplusplus tomlplusplus)
endif()
