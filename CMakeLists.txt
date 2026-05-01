cmake_minimum_required(VERSION 3.20)
project(SignalRoute VERSION 0.1.0 LANGUAGES CXX)

# ────────────────────────────────────────────
# Global settings
# ────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ────────────────────────────────────────────
# Build options and dependency discovery
# ────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(SignalRouteOptions)
include(SignalRouteSanitizers)
include(SignalRouteDependencies)

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
add_subdirectory(src/runtime)
add_subdirectory(src/transport/grpc)

# ────────────────────────────────────────────
# Main executable
# ────────────────────────────────────────────
add_executable(signalroute src/main.cpp)
target_link_libraries(signalroute PRIVATE
    sr_runtime
)

# ────────────────────────────────────────────
# Tests (optional)
# ────────────────────────────────────────────
if(SR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
