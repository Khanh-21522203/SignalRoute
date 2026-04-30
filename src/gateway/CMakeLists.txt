# Gateway service
add_library(sr_gateway STATIC
    admission_control.cpp
    gateway_service.cpp
    validator.cpp
    rate_limiter.cpp
)
target_include_directories(sr_gateway PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(sr_gateway PUBLIC sr_common)
