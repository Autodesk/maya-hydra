set(TARGET_NAME flowViewport)

add_library(${TARGET_NAME} SHARED)

# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME} 
    PRIVATE
        debugCodes.cpp
        global.cpp
        tokens.cpp
)

set(HEADERS
    api.h
    debugCodes.h
    global.h
    tokens.h
)

# -----------------------------------------------------------------------------
# include directories
# -----------------------------------------------------------------------------
target_include_directories(${TARGET_NAME} 
    PUBLIC
        ${PXR_INCLUDE_DIRS}
        ${CMAKE_BINARY_DIR}/include
        ${UFE_INCLUDE_DIR}
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
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> 
            DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)
endif()

# -----------------------------------------------------------------------------
# subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(colorPreferences)
add_subdirectory(sceneIndex)
add_subdirectory(API)
add_subdirectory(selection)
