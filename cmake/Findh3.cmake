# Finds the H3 C library from package-manager installs when the packaged
# h3Config.cmake is unusable or unavailable.
#
# Provides:
#   h3::h3
#   h3

include(FindPackageHandleStandardArgs)

find_path(H3_INCLUDE_DIR
    NAMES h3api.h
    PATH_SUFFIXES h3)

find_library(H3_LIBRARY
    NAMES h3)

find_package_handle_standard_args(h3
    REQUIRED_VARS
        H3_INCLUDE_DIR
        H3_LIBRARY)

if(h3_FOUND)
    if(NOT TARGET h3::h3)
        add_library(h3::h3 UNKNOWN IMPORTED)
        set_target_properties(h3::h3 PROPERTIES
            IMPORTED_LOCATION "${H3_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${H3_INCLUDE_DIR}")
    endif()

    if(NOT TARGET h3)
        add_library(h3 ALIAS h3::h3)
    endif()
endif()

mark_as_advanced(H3_INCLUDE_DIR H3_LIBRARY)
