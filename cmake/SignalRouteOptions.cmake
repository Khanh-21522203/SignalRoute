# SignalRoute build options.
# Fallback mode is the default so local unit tests do not require external services
# or generated code. Production adapters are enabled explicitly per dependency.

option(SR_BUILD_TESTS "Build SignalRoute tests" ON)

option(SR_ENABLE_PROTOBUF_GRPC "Generate and link protobuf/gRPC code" OFF)
option(SR_ENABLE_REAL_H3 "Use the real H3 library instead of the deterministic fallback" OFF)
option(SR_ENABLE_REAL_REDIS "Use a real Redis client adapter instead of the in-memory fallback" OFF)
option(SR_ENABLE_REAL_POSTGIS "Use libpq/PostGIS adapter instead of the in-memory fallback" OFF)
option(SR_ENABLE_REAL_KAFKA "Use librdkafka adapter instead of the in-memory fallback" OFF)
option(SR_ENABLE_PROMETHEUS "Use prometheus-cpp for metrics export" OFF)
option(SR_ENABLE_TOMLPLUSPLUS "Use toml++ for config parsing" OFF)

set(SR_DEPENDENCY_PROVIDER "system" CACHE STRING "Dependency provider for production adapters: system, vcpkg, conan, or fetchcontent")
set_property(CACHE SR_DEPENDENCY_PROVIDER PROPERTY STRINGS system vcpkg conan fetchcontent)
