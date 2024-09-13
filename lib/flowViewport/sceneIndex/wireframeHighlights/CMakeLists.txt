# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    baseWireframeHighlightSi.cpp
    meshWireframeHighlightSi.cpp
    piInstancerWireframeHighlightSi.cpp
    niPrototypeWireframeHighlightSi.cpp
)

set(HEADERS
    baseWireframeHighlightSi.h
    meshWireframeHighlightSi.h
    piInstancerWireframeHighlightSi.h
    niPrototypeWireframeHighlightSi.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/sceneIndex/wireframeHighlights
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/flowViewport/sceneIndex/wireframeHighlights
)
