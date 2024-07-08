set(TARGET_NAME flowViewportShadersDiscoveryPlugin)

add_library(${TARGET_NAME} SHARED)

# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME} 
    PRIVATE
        fvpShadersDiscoveryPlugin.cpp
)

set(HEADERS
    fvpShadersDiscoveryPlugin.h
)

# -----------------------------------------------------------------------------
# include directories
# -----------------------------------------------------------------------------
target_include_directories(${TARGET_NAME} 
    PUBLIC
        ${PXR_INCLUDE_DIRS}
        ${CMAKE_BINARY_DIR}/include
)

# -----------------------------------------------------------------------------
# compiler configuration
# -----------------------------------------------------------------------------
target_compile_definitions(${TARGET_NAME} 
    PRIVATE
        FVP_CUSTOMSHADERS_EXPORT
        $<$<BOOL:${IS_MACOSX}>:OSMac_>
        # Copy-pasted from lib/mayaUsd/CMakeLists.txt
)

mayaHydra_compile_config(${TARGET_NAME})

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME} 
    PRIVATE
        ar
        gf
        tf
		hd
        hdx
        sdf
        usdImaging
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------

if (IS_WINDOWS)
    configure_file("plugInfo_Win.json" ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json) 
else
(
	if (IS_LINUX)
		configure_file("plugInfo_Linux.json" ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json)
	endif()
	if (IS_MACOSX)
		configure_file("plugInfo_OSX.json" ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json)
	endif()
)
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/usd/flowViewportShadersDiscoveryPlugin/resources)
			
install(TARGETS ${TARGET_NAME}
        LIBRARY
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/usd/flowViewportShadersDiscoveryPlugin
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/usd/flowViewportShadersDiscoveryPlugin
)

if(IS_WINDOWS)
    if (NOT CODE_COVERAGE)
        install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> 
                DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)
    endif()
endif()

add_subdirectory(shaders)