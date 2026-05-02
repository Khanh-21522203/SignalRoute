# Finds the hiredis C client from package-manager installs.
#
# Provides:
#   hiredis::hiredis
#   hiredis

include(FindPackageHandleStandardArgs)

find_path(HIREDIS_INCLUDE_DIR
    NAMES hiredis/hiredis.h)

find_library(HIREDIS_LIBRARY
    NAMES hiredis)

find_package_handle_standard_args(Hiredis
    REQUIRED_VARS
        HIREDIS_INCLUDE_DIR
        HIREDIS_LIBRARY)

if(Hiredis_FOUND)
    if(NOT TARGET hiredis::hiredis)
        add_library(hiredis::hiredis UNKNOWN IMPORTED)
        set_target_properties(hiredis::hiredis PROPERTIES
            IMPORTED_LOCATION "${HIREDIS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${HIREDIS_INCLUDE_DIR}")
    endif()

    if(NOT TARGET hiredis)
        add_library(hiredis ALIAS hiredis::hiredis)
    endif()
endif()

mark_as_advanced(HIREDIS_INCLUDE_DIR HIREDIS_LIBRARY)
