# -----------------------------------------------------------------------------
# set target name
# -----------------------------------------------------------------------------
set(TARGET_NAME mayaHydraSceneBrowser)

# -----------------------------------------------------------------------------
# Qt safeguard
# -----------------------------------------------------------------------------
if (NOT Qt6_FOUND)
    message(WARNING "No Qt6 package found. Cannot build ${TARGET_NAME}.")
    return()
endif()

# -----------------------------------------------------------------------------
# add library
# -----------------------------------------------------------------------------
add_library(${TARGET_NAME} SHARED)

# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
        mayaHydraSceneBrowser.cpp
)

# -----------------------------------------------------------------------------
# compiler configuration
# -----------------------------------------------------------------------------
mayaHydra_compile_config(${TARGET_NAME})

target_compile_definitions(${TARGET_NAME}
    PRIVATE
        $<$<BOOL:${IS_MACOSX}>:OSMac_>
        $<$<BOOL:${IS_LINUX}>:LINUX>
)

# -----------------------------------------------------------------------------
# include directories
# -----------------------------------------------------------------------------
target_include_directories(${TARGET_NAME} 
    PUBLIC
        ${PXR_INCLUDE_DIRS}
        ${MAYA_INCLUDE_DIRS}
)

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME}
    PUBLIC 
        ${MAYA_LIBRARIES}
        mayaHydraLib
        adskHydraSceneBrowser
)

# -----------------------------------------------------------------------------
# properties
# -----------------------------------------------------------------------------
maya_set_plugin_properties(${TARGET_NAME})

# -----------------------------------------------------------------------------
# run-time search paths
# -----------------------------------------------------------------------------
if(IS_MACOSX OR IS_LINUX) 
    mayaUsd_init_rpath(rpath "lib/maya")
    if(DEFINED MAYAHYDRA_TO_USD_RELATIVE_PATH)
        mayaUsd_add_rpath(rpath "../../${MAYAHYDRA_TO_USD_RELATIVE_PATH}/lib")
    elseif(DEFINED PXR_USD_LOCATION)
        mayaUsd_add_rpath(rpath "${PXR_USD_LOCATION}/lib")
    endif()
    if (IS_LINUX AND DEFINED MAYAHYDRA_TO_USD_RELATIVE_PATH)
        mayaUsd_add_rpath(rpath "../../${MAYAHYDRA_TO_USD_RELATIVE_PATH}/lib64")
    endif()
    if(IS_MACOSX AND DEFINED MAYAHYDRA_TO_USD_RELATIVE_PATH)
        mayaUsd_add_rpath(rpath "../../../../Maya.app/Contents/MacOS")
    endif()
    mayaUsd_add_rpath(rpath "../")
    mayaUsd_install_rpath(rpath ${TARGET_NAME})
endif()

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(TARGETS ${TARGET_NAME}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/maya)