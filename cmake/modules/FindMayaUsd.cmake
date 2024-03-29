#
# Simple module to find MayaUsd.
#
# This module searches for a valid MayaUsd installation.
# It searches for MayaUsd's libraries and include header files.
#
# Variables that will be defined:
# MayaUsd_FOUND           Defined if a MayaUsd installation has been detected
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

find_library(MAYAUSDAPI_LIBRARY
    NAMES
        mayaUsdAPI
    HINTS
        ${MAYAUSD_LOCATION}
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


# Handle the QUIETLY and REQUIRED arguments and set MayaUsd_FOUND to TRUE if
# all listed variables are TRUE.
include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(MayaUsd
    REQUIRED_VARS
        MAYAUSD_INCLUDE_DIR
        MAYAUSDAPI_LIBRARY
        #MAYAUSD_MOD_PATH is not part of MayaUsd cuts downloaded so remove it from the requirements
        # as we want to be able to build with MayaUsd cuts.
)

if(MayaUsd_FOUND)
    message(STATUS "MayaUsd include dir: ${MAYAUSD_INCLUDE_DIR}")
    message(STATUS "MayaUsdAPI library fullpath : ${MAYAUSDAPI_LIBRARY}")
    message(STATUS "MAYAUSD_MOD_PATH : ${MAYAUSD_MOD_PATH}")
endif()

