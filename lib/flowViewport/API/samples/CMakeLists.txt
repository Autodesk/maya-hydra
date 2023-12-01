# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpSelectionClientExample.cpp
    fvpInformationClientExample.cpp
    fvpDataProducerSceneIndexExample.cpp
    fvpFilteringSceneIndexExample.cpp
    fvpFilteringSceneIndexClientExample.cpp
)

set(HEADERS
    fvpSelectionClientExample.h
    fvpInformationClientExample.h
    fvpDataProducerSceneIndexExample.h
    fvpFilteringSceneIndexExample.h
    fvpFilteringSceneIndexClientExample.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/API/samples
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/flowViewport/API/samples
)
