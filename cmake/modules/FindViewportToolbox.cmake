#
# Simple module to find ViewportToolbox.
#
# This module searches for a valid ViewportToolbox installation.
# It searches for ViewportToolbox's libraries and include header files.
#
# Variables that will be defined:
# ViewportToolbox_FOUND                     Defined if a ViewportToolbox installation has been detected
# VIEWPORTTOOLBOX_INCLUDE_DIR               Path to the ViewportToolbox include directory
# VIEWPORTTOOLBOX_VIEWPORTTOOLBOX_LIB       Path to ViewportToolbox library
# VIEWPORTTOOLBOX_VIEWPORTTOOLBOX_DLL_PATH  Path To ViewportToolbox DLL
# VIEWPORTTOOLBOX_VISUALSTYLES_LIB          Path to VisualStyles library

find_path(VIEWPORTTOOLBOX_INCLUDE_DIR
    AGP/ViewportToolbox/Exports.h
    HINTS
        ${VIEWPORTTOOLBOX_LOCATION}
    PATH_SUFFIXES
        include/
    DOC
        "ViewportToolbox headers path"
)

find_library(VIEWPORTTOOLBOX_VIEWPORTTOOLBOX_LIB
    NAMES
        agp_viewport_toolbox
    HINTS
        ${VIEWPORTTOOLBOX_LOCATION}
    PATH_SUFFIXES
       lib/
    DOC
        "ViewportToolbox library"
    NO_DEFAULT_PATH
)

find_path(VIEWPORTTOOLBOX_VIEWPORTTOOLBOX_DLL_PATH
    agp_viewport_toolbox.dll
    HINTS
        ${VIEWPORTTOOLBOX_LOCATION}
    PATH_SUFFIXES
        bin/
    DOC
        "ViewportToolbox DLL path"
)

find_library(VIEWPORTTOOLBOX_VISUALSTYLES_LIB
    NAMES
        agp_visual_styles
    HINTS
        ${VIEWPORTTOOLBOX_LOCATION}
    PATH_SUFFIXES
       lib/
    DOC
        "VisualStyles library"
    NO_DEFAULT_PATH
)

# Handle the QUIETLY and REQUIRED arguments and set ViewportToolbox_FOUND to TRUE if
# all listed variables are TRUE.
include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(ViewportToolbox
    REQUIRED_VARS
        VIEWPORTTOOLBOX_INCLUDE_DIR
        VIEWPORTTOOLBOX_VIEWPORTTOOLBOX_LIB
        VIEWPORTTOOLBOX_VIEWPORTTOOLBOX_DLL_PATH
        VIEWPORTTOOLBOX_VISUALSTYLES_LIB
)

if(ViewportToolbox_FOUND)
    message(STATUS "ViewportToolbox include dir: ${VIEWPORTTOOLBOX_INCLUDE_DIR}")
    message(STATUS "ViewportToolbox library fullpath : ${VIEWPORTTOOLBOX_VIEWPORTTOOLBOX_LIB}")
    message(STATUS "ViewportToolbox DLL path : ${VIEWPORTTOOLBOX_VIEWPORTTOOLBOX_DLL_PATH}")
    message(STATUS "VisualStyles library fullpath : ${VIEWPORTTOOLBOX_VISUALSTYLES_LIB}")
endif()
