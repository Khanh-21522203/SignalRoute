# Finds librdkafka C and C++ libraries from package-manager installs that expose
# pkg-config files but no RdKafkaConfig.cmake package.
#
# Provides:
#   RdKafka::rdkafka
#   RdKafka::rdkafka++
#   rdkafka
#   rdkafka++

include(FindPackageHandleStandardArgs)
find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(RDKAFKA QUIET rdkafka)
    pkg_check_modules(RDKAFKA_CPP QUIET rdkafka++)
endif()

find_path(RDKAFKA_INCLUDE_DIR
    NAMES rdkafka.h
    HINTS ${RDKAFKA_INCLUDE_DIRS}
    PATH_SUFFIXES librdkafka)

find_path(RDKAFKA_CPP_INCLUDE_DIR
    NAMES rdkafkacpp.h
    HINTS ${RDKAFKA_CPP_INCLUDE_DIRS}
    PATH_SUFFIXES librdkafka)

find_library(RDKAFKA_LIBRARY
    NAMES rdkafka
    HINTS ${RDKAFKA_LIBRARY_DIRS})

find_library(RDKAFKA_CPP_LIBRARY
    NAMES rdkafka++
    HINTS ${RDKAFKA_CPP_LIBRARY_DIRS})

find_package_handle_standard_args(RdKafka
    REQUIRED_VARS
        RDKAFKA_INCLUDE_DIR
        RDKAFKA_CPP_INCLUDE_DIR
        RDKAFKA_LIBRARY
        RDKAFKA_CPP_LIBRARY)

if(RdKafka_FOUND)
    if(NOT TARGET RdKafka::rdkafka)
        add_library(RdKafka::rdkafka UNKNOWN IMPORTED)
        set_target_properties(RdKafka::rdkafka PROPERTIES
            IMPORTED_LOCATION "${RDKAFKA_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${RDKAFKA_INCLUDE_DIR}")
        if(RDKAFKA_LINK_LIBRARIES)
            set_property(TARGET RdKafka::rdkafka PROPERTY
                INTERFACE_LINK_LIBRARIES "${RDKAFKA_LINK_LIBRARIES}")
        endif()
    endif()

    if(NOT TARGET RdKafka::rdkafka++)
        add_library(RdKafka::rdkafka++ UNKNOWN IMPORTED)
        set_target_properties(RdKafka::rdkafka++ PROPERTIES
            IMPORTED_LOCATION "${RDKAFKA_CPP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${RDKAFKA_CPP_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES RdKafka::rdkafka)
        if(RDKAFKA_CPP_LINK_LIBRARIES)
            set_property(TARGET RdKafka::rdkafka++ APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES "${RDKAFKA_CPP_LINK_LIBRARIES}")
        endif()
    endif()

    if(NOT TARGET rdkafka)
        add_library(rdkafka ALIAS RdKafka::rdkafka)
    endif()

    if(NOT TARGET rdkafka++)
        add_library(rdkafka++ ALIAS RdKafka::rdkafka++)
    endif()
endif()

mark_as_advanced(
    RDKAFKA_INCLUDE_DIR
    RDKAFKA_CPP_INCLUDE_DIR
    RDKAFKA_LIBRARY
    RDKAFKA_CPP_LIBRARY)
