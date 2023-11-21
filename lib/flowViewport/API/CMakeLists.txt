set(HEADERS
    fvpSelectionClient.h
    fvpFlowSelectionInterface.h
    fvpVersionInterface.h
    fvpInformationInterface.h
    fvpInformationClient.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/API
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/flowViewport/API
)

# -----------------------------------------------------------------------------
# subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(interfacesImp)
add_subdirectory(samples)
add_subdirectory(perViewportSceneIndicesData)
