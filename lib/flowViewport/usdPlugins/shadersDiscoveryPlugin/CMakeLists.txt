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

set(PLUG_INFO_FILE "plugInfo.json")

if(IS_WINDOWS)
  set(PLUG_INFO_PATH "Windows/${PLUG_INFO_FILE}")
else(
  if(IS_MACOSX OR IS_LINUX) 
    set(PLUG_INFO_PATH "Linux-OSX/${PLUG_INFO_FILE}")
  endif()
)
endif()

configure_file(${PLUG_INFO_PATH} ${CMAKE_CURRENT_BINARY_DIR}/${PLUG_INFO_FILE})

install(FILES ${PLUG_INFO_PATH}
        DESTINATION 
            ${CMAKE_INSTALL_PREFIX}/lib/usd/flowViewportShadersDiscoveryPlugin/resources)
			
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