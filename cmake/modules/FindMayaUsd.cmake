#
# Simple module to find MayaUsd.
#
# This module searches for a valid MayaUsd installation.
# It searches for MayaUsd's libraries and include header files.
#
# Variables that will be defined:
# MAYAUSD_FOUND           Defined if a MayaUsd installation has been detected
# MAYAUSD_LIBRARY_DIR     Path to MayaUsd libraries directory
# MAYAUSD_INCLUDE_DIR     Path to the MayaUsd include directory
# MAYAUSDAPI_LIBRARY      Path to MayaUsdAPI library
# MAYAUSD_MOD_PATH        Path to mayaUSD.mod file
#

find_path(MAYAUSD_INCLUDE_DIR
        mayaUsd/mayaUsd.h
    HINTS
        ${MAYAUSD_LOCATION}
    PATH_SUFFIXES
        include/
    DOC
        "MayaUsd header path"
)

find_path(MAYAUSD_LIBRARY_DIR
        mayaUsd.lib
    HINTS
        ${MAYAUSD_LOCATION}
    PATH_SUFFIXES
        lib/
    DOC
        "MayaUsd libraries path"
)

find_library(MAYAUSDAPI_LIBRARY
    NAMES
        mayaUsdAPI
    HINTS
        ${MAYAUSD_LOCATION}
    PATHS
        ${MAYAUSD_LIBRARY_DIR}
    PATH_SUFFIXES
        lib/
    DOC
        "mayaUsdAPI library"
    NO_DEFAULT_PATH
)

find_path(MAYAUSD_MOD_PATH
        mayaUSD.mod
    HINTS
        ${MAYAUSD_LOCATION}
    DOC
        "mayaUSD.mod path"
)


# Handle the QUIETLY and REQUIRED arguments and set MAYAUSD_FOUND to TRUE if
# all listed variables are TRUE.
include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(MAYAUSD
    REQUIRED_VARS
        MAYAUSD_INCLUDE_DIR
        MAYAUSD_LIBRARY_DIR
)

if(MAYAUSD_FOUND)
    message(STATUS "MayaUsd include dir: ${MAYAUSD_INCLUDE_DIR}")
    message(STATUS "MayaUsd libraries dir: ${MAYAUSD_LIBRARY_DIR}")
    message(STATUS "MayaUsdAPI.lib fullpath : ${MAYAUSDAPI_LIBRARY}")
    message(STATUS "MAYAUSD_MOD_PATH : ${MAYAUSD_MOD_PATH}")
    
    #Add MAYAUSD_MOD_PATH (the path where maya USD .mod file is) to the MAYA_MODULE_PATH
    
    # Get the current value of the environment variable
    set(CURRENT_MAYA_MODULE_PATH $ENV{MAYA_MODULE_PATH})

    # Append the new path to the current value
    set(MAYA_MODULE_PATH "${CURRENT_MAYA_MODULE_PATH}:${MAYAUSD_MOD_PATH}")

    # Export the new value to the environment
    set(ENV{MAYA_MODULE_PATH} ${MAYA_MODULE_PATH})
else()
    if(DEFINED MAYAUSD_LOCATION)
        message(FATAL_ERROR "Could not find MayaUsd library and include directories in the folder provided : ${MAYAUSD_LOCATION}")
    endif()
endif()

