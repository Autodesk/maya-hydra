#
# Copyright 2020 Autodesk
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
cmake_minimum_required(VERSION 3.13...3.17)

project(maya-usd)

#------------------------------------------------------------------------------
# options
#------------------------------------------------------------------------------
option(BUILD_TESTS "Build tests." ON)
option(BUILD_STRICT_MODE "Enforce all warnings as errors." ON)
option(BUILD_SHARED_LIBS "Build libraries as shared or static." ON)
if(APPLE)
    option(BUILD_UB2 "Build Universal Binary 2 (UB2) Intel64+arm64" OFF)
endif()
set(BUILD_WITH_PYTHON_3_VERSION 3.10 CACHE STRING "The version of Python 3 to build with")
option(CMAKE_WANT_MATERIALX_BUILD "Enable building with MaterialX (experimental)." OFF)

set(PXR_OVERRIDE_PLUGINPATH_NAME PXR_PLUGINPATH_NAME
    CACHE STRING "Name of env var USD searches to find plugins")
    
# Build-related information, expected to be passed in by the parent build.
set(MAYAHYDRA_BUILD_NUMBER 0 CACHE STRING "Build number.")
set(MAYAHYDRA_GIT_COMMIT "Unknown_Git_commit" CACHE STRING "Build commit.")
set(MAYAHYDRA_GIT_BRANCH "Unknown_Git_branch" CACHE STRING "Build branch.")

#------------------------------------------------------------------------------
# internal flags to control build
#------------------------------------------------------------------------------
# MAYAHYDRA_TO_USD_RELATIVE_PATH : Set this variable to any relative path from install
#                                  folder to USD location. If defined will set relative
#                                  rpaths for USD libraries.

#------------------------------------------------------------------------------
# global options
#------------------------------------------------------------------------------
# Avoid noisy install messages
set(CMAKE_INSTALL_MESSAGE "NEVER")

if(APPLE)
    if(BUILD_UB2)
        message(STATUS "Building with Universal Binary 2")
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
        set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
    else()
        set(CMAKE_OSX_ARCHITECTURES "x86_64")
        set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14)
    endif()
endif()

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
)

# Use RUNPATH instead of RPATH for all shared libs, executables and modules on Linux
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux") # IS_LINUX not yet defined
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--enable-new-dtags")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--enable-new-dtags")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--enable-new-dtags")
endif()

#------------------------------------------------------------------------------
# modules and definitions
#------------------------------------------------------------------------------
include(cmake/utils.cmake)
find_package(Maya 2024 REQUIRED)

if(APPLE AND BUILD_UB2 AND NOT MAYA_MACOSX_BUILT_WITH_UB2)
    message(WARNING  "Maya was NOT built with Universal Binary 2")
endif()

include(cmake/mayahydra_version.info)
set(MAYAHYDRA_VERSION "${MAYAHYDRA_MAJOR_VERSION}.${MAYAHYDRA_MINOR_VERSION}.${MAYAHYDRA_PATCH_LEVEL}")

include(cmake/flowViewport_version.info)
set(FLOWVIEWPORT_VERSION "${FLOWVIEWPORT_MAJOR_VERSION}.${FLOWVIEWPORT_MINOR_VERSION}.${FLOWVIEWPORT_PATCH_LEVEL}")

if (DEFINED PYTHON_INCLUDE_DIR AND DEFINED PYTHON_LIBRARIES AND DEFINED Python_EXECUTABLE)
    SET(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
    SET(PYTHONLIBS_FOUND TRUE)
    # Use the Python module to find the python lib.
    find_package(Python ${BUILD_WITH_PYTHON_3_VERSION} EXACT REQUIRED COMPONENTS Interpreter)
    if(NOT Python_Interpreter_FOUND)
        set(PYTHONLIBS_FOUND FALSE)
    endif()
endif()
if (NOT PYTHONLIBS_FOUND)
    include(cmake/python.cmake)
endif()
message(STATUS "Build mayaHydra with Python3 = " ${BUILD_WITH_PYTHON_3_VERSION})
message(STATUS "   PYTHON_INCLUDE_DIRS = ${PYTHON_INCLUDE_DIRS}")
message(STATUS "   PYTHON_LIBRARIES    = ${PYTHON_LIBRARIES}")
message(STATUS "   Python_EXECUTABLE   = ${Python_EXECUTABLE}")

include(cmake/jinja.cmake)

find_package(USD 0.22.11 REQUIRED)
if (CMAKE_WANT_MATERIALX_BUILD)
    # Requires at least USD 22.11 for hdMtlx module and USD must have been built with MaterialX:
    if(NOT TARGET hdMtlx)
        set(CMAKE_WANT_MATERIALX_BUILD OFF)
        message(WARNING "Disabling MaterialX VP2 rendering: it is not supported by this USD package.")
    endif()
endif()
include(cmake/usd.cmake)

option(BUILD_MAYAHYDRALIB "Build the Maya-To-Hydra plugin and scene delegate." ON)

find_package(UFE REQUIRED)
if(UFE_FOUND)
    message(STATUS "Building with UFE ${UFE_VERSION}.")
else()
    message(STATUS "UFE not found.")
endif()

find_package(MAYAUSD OPTIONAL_COMPONENTS)
if(MAYAUSD_FOUND)
    message(STATUS "Building with MayaUsd.")
else()
    message(STATUS "MayaUsd not used to build.")
endif()

if(CMAKE_WANT_MATERIALX_BUILD AND MAYA_LIGHTAPI_VERSION LESS 2)
    set(CMAKE_WANT_MATERIALX_BUILD OFF)
    message(WARNING "Disabling MaterialX VP2 rendering: it is not supported by this version of Maya.")
endif()

if(TARGET hdMtlx)
    # MaterialX was built into USD. We need to know where to find MaterialX targets.
    find_package(MaterialX REQUIRED)
    if(TARGET MaterialXCore)
        message(STATUS "Found MaterialX")
    endif()
endif()

if(MAYA_APP_VERSION VERSION_GREATER 2024)
    # Look for Qt6 in the Maya devkit.
    # The Qt6 archive in the Maya devkit contains everything needed for the normal cmake find_package.
    set(CMAKE_PREFIX_PATH "${MAYA_DEVKIT_LOCATION}/Qt")
    set(WANT_QT_VERSION 6.5)
    find_package(Qt6 ${WANT_QT_VERSION} COMPONENTS Core Widgets QUIET)
    if (Qt6_FOUND)
        message(STATUS "Found Qt ${Qt6_VERSION} in Maya devkit. Hydra Scene Browser will be built.")
        set(BUILD_HDSB_PLUGIN TRUE)
    else()
        message(WARNING "Could not find Qt ${WANT_QT_VERSION} in Maya devkit directory: ${MAYA_DEVKIT_LOCATION}. \
                        You must extract Qt.tar.gz. Hydra Scene Browser will not be built.")
    endif()
else()
    message(WARNING "Maya ${MAYA_APP_VERSION} does not contain Qt6 in its devkit. \
                    Hydra Scene Browser will not be built.")
endif()

#------------------------------------------------------------------------------
# compiler configuration
#------------------------------------------------------------------------------
include(cmake/compiler_config.cmake)

#------------------------------------------------------------------------------
# gulrak filesystem
#------------------------------------------------------------------------------
include(cmake/gulrak.cmake)

#------------------------------------------------------------------------------
# test
#------------------------------------------------------------------------------
if (BUILD_TESTS)
    include(cmake/googletest.cmake)
    include(cmake/test.cmake)
    fetch_googletest()
    enable_testing()
    add_subdirectory(test)
endif()

#------------------------------------------------------------------------------
# lib
#------------------------------------------------------------------------------
add_subdirectory(lib)

if(BUILD_HDSB_PLUGIN)
    add_subdirectory(lib/adskHydraSceneBrowser)
endif()

#------------------------------------------------------------------------------
# plugin
#------------------------------------------------------------------------------
if(BUILD_HDSB_PLUGIN)
    add_subdirectory(plugin/mayaHydraSceneBrowser)
    if (BUILD_TESTS)
        add_subdirectory(plugin/mayaHydraSceneBrowserTest)
    endif()
endif()

#------------------------------------------------------------------------------
# install
#------------------------------------------------------------------------------
if (DEFINED MAYAHYDRA_TO_USD_RELATIVE_PATH)
    set(USD_INSTALL_LOCATION "${CMAKE_INSTALL_PREFIX}/${MAYAHYDRA_TO_USD_RELATIVE_PATH}")
else()
    set(USD_INSTALL_LOCATION ${PXR_USD_LOCATION})
endif()

#------------------------------------------------------------------------------
# Maya module files
#------------------------------------------------------------------------------
if (IS_WINDOWS)
    configure_file("modules/mayaHydra_Win.mod.template" ${PROJECT_BINARY_DIR}/mayaHydra.mod)
else()
    configure_file("modules/mayaHydra.mod.template" ${PROJECT_BINARY_DIR}/mayaHydra.mod)
endif()

install(FILES ${PROJECT_BINARY_DIR}/mayaHydra.mod DESTINATION ${CMAKE_INSTALL_PREFIX})
