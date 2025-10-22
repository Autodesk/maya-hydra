set(TARGET_NAME flowViewport)

add_library(${TARGET_NAME} SHARED)

# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME} 
    PRIVATE
        debugCodes.cpp
        fvpInstruments.cpp
        fvpPrimUtils.cpp
        fvpUtils.cpp
        global.cpp
        tokens.cpp
		fvpPurposeRenderTagsForPasses.cpp
		fvpFramePassData.cpp
)

set(HEADERS
    api.h
    debugCodes.h
    fvpInstruments.h
    fvpPrimUtils.h
    fvpUtils.h
    global.h
    tokens.h
    fvpWireframeColorInterface.h
	fvpPurposeRenderTagsForPasses.h
	fvpFramePassData.h
)

# -----------------------------------------------------------------------------
# include directories
# -----------------------------------------------------------------------------
target_include_directories(${TARGET_NAME} 
    PUBLIC
        ${PXR_INCLUDE_DIRS}
        ${CMAKE_BINARY_DIR}/include
        ${UFE_INCLUDE_DIR}
        $<$<BOOL:${BUILD_WITH_VIEWPORT_TOOLBOX}>:${VIEWPORTTOOLBOX_INCLUDE_DIR}>
)

# -----------------------------------------------------------------------------
# compiler configuration
# -----------------------------------------------------------------------------
target_compile_definitions(${TARGET_NAME} 
    PRIVATE
        FVP_EXPORT
        $<$<BOOL:${IS_MACOSX}>:OSMac_>
        # Copy-pasted from lib/mayaUsd/CMakeLists.txt
)

mayaHydra_compile_config(${TARGET_NAME})

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME} 
    PUBLIC
        hd
        hdx
        sdf
        usdImaging
        ${UFE_LIBRARY}
        $<$<BOOL:${BUILD_WITH_VIEWPORT_TOOLBOX}>:${VIEWPORTTOOLBOX_VIEWPORTTOOLBOX_LIB}>
        $<$<BOOL:${BUILD_WITH_VIEWPORT_TOOLBOX}>:${VIEWPORTTOOLBOX_VISUALSTYLES_LIB}>
    PRIVATE
        ar
        gf
        tf
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
set(SRCFILE ${CMAKE_CURRENT_SOURCE_DIR}/flowViewport.h.src)
set(DSTFILE ${CMAKE_BINARY_DIR}/include/flowViewport/flowViewport.h)
configure_file(${SRCFILE} ${DSTFILE})

mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------

install(TARGETS ${TARGET_NAME}
        LIBRARY
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

install(FILES ${HEADERS}
        DESTINATION 
            ${CMAKE_INSTALL_PREFIX}/include/flowViewport
)

install(FILES ${CMAKE_BINARY_DIR}/include/flowViewport/flowViewport.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/flowViewport
)

if(IS_WINDOWS)
    if (NOT CODE_COVERAGE)
        install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> 
                DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)
    endif()
endif()

if (BUILD_WITH_VIEWPORT_TOOLBOX AND IS_WINDOWS)
    install(
        FILES 
            ${VIEWPORTTOOLBOX_VIEWPORTTOOLBOX_DLL_PATH}/hvt.dll
            ${VIEWPORTTOOLBOX_VIEWPORTTOOLBOX_DLL_PATH}/hvt.pdb
        DESTINATION 
            ${CMAKE_INSTALL_PREFIX}/lib
    )
endif()

# -----------------------------------------------------------------------------
# subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(colorPreferences)
add_subdirectory(sceneIndex)
add_subdirectory(API)
add_subdirectory(selection)
add_subdirectory(usdPlugins)
add_subdirectory(imageWriter)
