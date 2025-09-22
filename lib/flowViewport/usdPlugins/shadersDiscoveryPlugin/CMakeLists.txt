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

# Generate plugInfo.json with the correct base class depending on USD version
# and the proper library name depending on the platform.
if (PXR_VERSION VERSION_GREATER_EQUAL 2508)
    set(BASE_CLASS_NAME "SdrDiscoveryPlugin")
else()
    set(BASE_CLASS_NAME "NdrDiscoveryPlugin")
endif()

if (IS_WINDOWS)
    set(LIBRARY_PATH "flowViewportShadersDiscoveryPlugin.dll")
elseif(IS_LINUX)
    set(LIBRARY_PATH "libflowViewportShadersDiscoveryPlugin.so")
elseif(IS_MACOSX)
    set(LIBRARY_PATH "libflowViewportShadersDiscoveryPlugin.dylib")
endif()

configure_file("plugInfo.json.in" ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json @ONLY)

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