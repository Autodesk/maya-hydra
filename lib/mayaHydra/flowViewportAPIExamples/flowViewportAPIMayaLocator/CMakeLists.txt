set(TARGET_NAME flowViewportAPIMayaLocator)

add_library(${TARGET_NAME} SHARED)

# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME} 
    PRIVATE
        flowViewportAPIMayaLocator.cpp
)

set(HEADERS
	PRIVATE
)

# -----------------------------------------------------------------------------
# compiler configuration
# -----------------------------------------------------------------------------
target_compile_definitions(${TARGET_NAME}
    PRIVATE 
        # M3dView needs this (technically, MNativeWindowHdl.h)
        # private atm because M3dView is only used in .cpp right now
        # gcc will NOT set this automatically
        $<$<BOOL:${IS_LINUX}>:LINUX>
        # Not sure if msvcc sets this automatically, but won't hurt to redefine
        $<$<BOOL:${IS_WINDOWS}>:_WIN32>
		MFB_ALT_PACKAGE_NAME="${TARGET_NAME}" # This is used by Pixar HdSceneIndexPluginRegistry::Define to create a unique name
)

# -----------------------------------------------------------------------------
# include directories
# -----------------------------------------------------------------------------
if(DEFINED MAYAUSD_VERSION)
    target_compile_definitions(${TARGET_NAME}
        PRIVATE
            MAYAUSD_VERSION=${MAYAUSD_VERSION}
    )
endif()

if(DEFINED MAYAHYDRA_VERSION)
    target_compile_definitions(${TARGET_NAME}
        PRIVATE
            MAYAHYDRA_VERSION=${MAYAHYDRA_VERSION}
    )
endif()
if(DEFINED MAYAHYDRA_CUT_ID)
    target_compile_definitions(${TARGET_NAME}
        PRIVATE
            MAYAHYDRA_CUT_ID=${MAYAHYDRA_CUT_ID}
    )
endif()

mayaHydra_compile_config(${TARGET_NAME})

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME} 
    PRIVATE 
        mayaHydraLib
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
    if(DEFINED MAYAUSD_TO_USD_RELATIVE_PATH)
        mayaUsd_add_rpath(rpath "../../${MAYAUSD_TO_USD_RELATIVE_PATH}/lib")
    elseif(DEFINED PXR_USD_LOCATION)
        mayaUsd_add_rpath(rpath "${PXR_USD_LOCATION}/lib")
    endif()
    if (IS_LINUX AND DEFINED MAYAUSD_TO_USD_RELATIVE_PATH)
        mayaUsd_add_rpath(rpath "../../${MAYAUSD_TO_USD_RELATIVE_PATH}/lib64")
    endif()
    if(IS_MACOSX AND DEFINED MAYAUSD_TO_USD_RELATIVE_PATH)
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

if(IS_WINDOWS)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> 
            DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/maya OPTIONAL)
endif()

set(LIBFILENAME ${CMAKE_SHARED_LIBRARY_PREFIX}${TARGET_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
set(PLUG_INFO_LIBRARY_PATH "../../../${LIBFILENAME}")
set(PLUG_INFO_RESOURCE_PATH "resources")