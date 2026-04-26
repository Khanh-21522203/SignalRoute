# Processor service
add_library(sr_processor STATIC
    processor_service.cpp
    dedup_window.cpp
    sequence_guard.cpp
    state_writer.cpp
    history_writer.cpp
    processing_loop.cpp
    processor_event_handlers.cpp
)
target_include_directories(sr_processor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(sr_processor PUBLIC sr_common)
