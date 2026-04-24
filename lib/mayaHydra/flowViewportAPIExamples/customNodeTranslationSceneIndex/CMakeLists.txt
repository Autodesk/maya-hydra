set(TARGET_NAME mayaHydraCustomNodeTranslationSceneIndex)

add_library(${TARGET_NAME} SHARED)

# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME} 
    PRIVATE
        customNodeTranslationSceneIndexPlugin.cpp
        customNodeTranslationSceneIndex.cpp
)

set(HEADERS
        customNodeTranslationSceneIndexPlugin.h
        customNodeTranslationSceneIndex.h
)

# -----------------------------------------------------------------------------
# compiler configuration
# -----------------------------------------------------------------------------
target_compile_definitions(${TARGET_NAME}
    PRIVATE 
        $<$<BOOL:${IS_LINUX}>:LINUX>
		MFB_ALT_PACKAGE_NAME="${TARGET_NAME}"
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

if(NOT DEFINED MAYAHYDRA_VERSION)
    message(FATAL_ERROR "No MAYAHYDRA_VERSION defined, aborting configuration." )
endif()

target_compile_definitions(${TARGET_NAME}
    PRIVATE
        MAYAHYDRA_VERSION=${MAYAHYDRA_VERSION}
)

mayaHydra_compile_config(${TARGET_NAME})

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME} 
    PRIVATE 
        gf
        tf
        hd
        hdx
        sdf
        usdImaging
        usdLux
)

# -----------------------------------------------------------------------------
# run-time search paths
# -----------------------------------------------------------------------------
if(IS_MACOSX OR IS_LINUX) 
    mayaUsd_init_rpath(rpath "lib/maya")
    if(DEFINED MAYAUSD_TO_USD_RELATIVE_PATH)
        mayaUsd_add_rpath(rpath "../../${MAYAUSD_TO_USD_RELATIVE_PATH}/lib")
        if (IS_LINUX)
            mayaUsd_add_rpath(rpath "../../${MAYAUSD_TO_USD_RELATIVE_PATH}/lib64")
        endif()
        if(IS_MACOSX)
            mayaUsd_add_rpath(rpath "../../../../Maya.app/Contents/MacOS")
        endif()
    elseif(DEFINED PXR_USD_LOCATION)
        mayaUsd_add_rpath(rpath "${PXR_USD_LOCATION}/lib")
    endif()
    
    mayaUsd_add_rpath(rpath "../")
    mayaUsd_install_rpath(rpath ${TARGET_NAME})
endif()

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
set(LIBFILENAME ${CMAKE_SHARED_LIBRARY_PREFIX}${TARGET_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
set(PLUG_INFO_PATH "plugInfo.json")
set(PLUG_INFO_LIBRARY_PATH "${LIBFILENAME}")
set(PLUG_INFO_RESOURCE_PATH "resources")

configure_file(${PLUG_INFO_PATH} ${CMAKE_CURRENT_BINARY_DIR}/${PLUG_INFO_PATH})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PLUG_INFO_PATH} 
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/usd/customNodeTranslationSceneIndex/resources)

install(TARGETS ${TARGET_NAME}
        LIBRARY
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/usd/customNodeTranslationSceneIndex
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/usd/customNodeTranslationSceneIndex
)

if(IS_WINDOWS)
    if (NOT CODE_COVERAGE)
        install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> 
                DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/maya OPTIONAL)    
    endif()                
endif()
