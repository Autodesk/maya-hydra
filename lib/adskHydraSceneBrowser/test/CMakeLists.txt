# -----------------------------------------------------------------------------
# set target name
# -----------------------------------------------------------------------------
set(TARGET_NAME adskHydraSceneBrowserTesting)

# -----------------------------------------------------------------------------
# Qt safeguard
# -----------------------------------------------------------------------------
if (NOT Qt6_FOUND)
    message(WARNING "No Qt6 package found. Cannot build ${TARGET_NAME}.")
    return()
endif()

# -----------------------------------------------------------------------------
# prerequisites
# -----------------------------------------------------------------------------
find_package(GTest REQUIRED)

# -----------------------------------------------------------------------------
# add library
# -----------------------------------------------------------------------------
add_library(${TARGET_NAME} SHARED)

# -----------------------------------------------------------------------------
# file lists
# -----------------------------------------------------------------------------
set(SHIPPED_HEADERS
    adskHydraSceneBrowserTesting.h
)

set(SOURCES
    adskHydraSceneBrowserTestFixture.cpp
    adskHydraSceneBrowserTesting.cpp
)

# -----------------------------------------------------------------------------
# compiler configuration
# -----------------------------------------------------------------------------
mayaHydra_compile_config(${TARGET_NAME}) # TODO : This will have to be changed when moving to a standalone repo.

target_compile_definitions(${TARGET_NAME}
    PUBLIC
        $<$<BOOL:${IS_LINUX}>:LINUX>
        $<$<BOOL:${IS_MACOSX}>:OSMac_>
        $<$<BOOL:${IS_WINDOWS}>:GTEST_LINKED_AS_SHARED_LIBRARY>
        $<$<STREQUAL:${CMAKE_BUILD_TYPE},Debug>:TBB_USE_DEBUG>
    PRIVATE
        HDUITEST_EXPORT
)

# -----------------------------------------------------------------------------
# include directories
# -----------------------------------------------------------------------------
target_include_directories(${TARGET_NAME} 
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GTEST_INCLUDE_DIRS}
)

# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
        ${SOURCES}
)

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME}
    PUBLIC
        adskHydraSceneBrowser
        ${GTEST_LIBRARIES}
)

# -----------------------------------------------------------------------------
# runtime search paths
# -----------------------------------------------------------------------------
# TODO : This will have to be changed when moving to a standalone repo.
if(IS_MACOSX OR IS_LINUX) 
    mayaUsd_init_rpath(rpath "lib")
    mayaUsd_add_rpath(rpath "${CMAKE_INSTALL_PREFIX}/lib/gtest")
    mayaUsd_install_rpath(rpath ${TARGET_NAME})
endif()

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(TARGETS ${TARGET_NAME}
        LIBRARY
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

list(TRANSFORM SHIPPED_HEADERS PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
install(FILES ${SHIPPED_HEADERS}
        DESTINATION 
            ${CMAKE_INSTALL_PREFIX}/include/adskHydraSceneBrowserTesting
)

if(IS_WINDOWS)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> 
            DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)
endif()
