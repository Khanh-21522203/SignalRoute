# Sanitizer profiles for local and CI verification.
# These options are dependency-free and apply to all targets configured after
# this module is included.

set(_sr_sanitizer_compile_options "")
set(_sr_sanitizer_link_options "")
set(_sr_enabled_sanitizers "")

if(SR_ENABLE_ASAN OR SR_ENABLE_UBSAN OR SR_ENABLE_TSAN)
    if(MSVC)
        message(FATAL_ERROR "SignalRoute sanitizer options currently support Clang/GCC-style compilers, not MSVC")
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "SignalRoute sanitizer options require Clang or GCC")
    endif()

    add_compile_options(-fno-omit-frame-pointer)
    add_link_options(-fno-omit-frame-pointer)
endif()

if(SR_ENABLE_ASAN)
    list(APPEND _sr_enabled_sanitizers "address")
endif()

if(SR_ENABLE_UBSAN)
    list(APPEND _sr_enabled_sanitizers "undefined")
endif()

if(SR_ENABLE_TSAN)
    list(APPEND _sr_enabled_sanitizers "thread")
endif()

if(_sr_enabled_sanitizers)
    list(JOIN _sr_enabled_sanitizers "," _sr_sanitizer_list)
    list(APPEND _sr_sanitizer_compile_options "-fsanitize=${_sr_sanitizer_list}")
    list(APPEND _sr_sanitizer_link_options "-fsanitize=${_sr_sanitizer_list}")

    add_compile_options(${_sr_sanitizer_compile_options})
    add_link_options(${_sr_sanitizer_link_options})
    message(STATUS "SignalRoute sanitizers enabled: ${_sr_sanitizer_list}")
else()
    message(STATUS "SignalRoute sanitizers: disabled")
endif()
