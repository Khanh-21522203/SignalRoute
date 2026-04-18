cmake_minimum_required(VERSION 3.20)
project(SignalRoute VERSION 0.1.0 LANGUAGES CXX)

# ────────────────────────────────────────────
# Global settings
# ────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ────────────────────────────────────────────
# Dependencies (TODO: integrate via vcpkg, Conan, or FetchContent)
# ────────────────────────────────────────────

# TODO: find_package(Protobuf REQUIRED)
# TODO: find_package(gRPC REQUIRED)
# TODO: find_package(hiredis REQUIRED)       # Redis C client
# TODO: find_package(redis++ REQUIRED)       # Redis C++ wrapper
# TODO: find_package(RdKafka REQUIRED)       # librdkafka
# TODO: find_package(PostgreSQL REQUIRED)    # libpq
# TODO: find_package(h3 REQUIRED)            # H3 spatial index
# TODO: find_package(tomlplusplus REQUIRED)  # TOML config parser
# TODO: find_package(prometheus-cpp REQUIRED) # Metrics

# ────────────────────────────────────────────
# Proto generation
# ────────────────────────────────────────────
add_subdirectory(proto)

# ────────────────────────────────────────────
# Shared common library
# ────────────────────────────────────────────
add_subdirectory(src/common)

# ────────────────────────────────────────────
# Service modules
# ────────────────────────────────────────────
add_subdirectory(src/gateway)
add_subdirectory(src/processor)
add_subdirectory(src/query)
add_subdirectory(src/geofence)
add_subdirectory(src/matching)
add_subdirectory(src/workers)

# ────────────────────────────────────────────
# Main executable
# ────────────────────────────────────────────
add_executable(signalroute src/main.cpp)
target_link_libraries(signalroute PRIVATE
    sr_common
    sr_gateway
    sr_processor
    sr_query
    sr_geofence
    sr_matching
    sr_workers
    # signalroute_proto  # TODO: uncomment when proto build is configured
)

# ────────────────────────────────────────────
# Tests (optional)
# ────────────────────────────────────────────
option(SR_BUILD_TESTS "Build SignalRoute tests" ON)
if(SR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
