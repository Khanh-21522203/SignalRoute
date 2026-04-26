# Matching server framework
add_library(sr_matching STATIC
    matching_service.cpp
    nearest_strategy.cpp
    strategy_registry.cpp
    reservation_manager.cpp
)
target_include_directories(sr_matching PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(sr_matching PUBLIC sr_common)
