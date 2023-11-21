# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpSelectionInterfaceImp.cpp
    fvpVersionInterfaceImp.cpp
    fvpInformationInterfaceImp.cpp
)

set(HEADERS
    fvpSelectionInterfaceImp.h
    fvpVersionInterfaceImp.h
    fvpInformationInterfaceImp.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/API/interfacesImp
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/flowViewport/API/interfacesImp
)
