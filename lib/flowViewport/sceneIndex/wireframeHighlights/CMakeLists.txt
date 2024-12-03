# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    baseWireframeHighlightSi.cpp
    meshWireframeHighlightSi.cpp
    piInstancerWireframeHighlightSi.cpp
    piPrototypeWireframeHighlightSi.cpp
    niInstanceWireframeHighlightSi.cpp
    niPrototypeWireframeHighlightSi.cpp
    baseWhSi.cpp
    #meshWhSi.cpp
    piInstancerWhSi.cpp
)

set(HEADERS
    baseWireframeHighlightSi.h
    meshWireframeHighlightSi.h
    piInstancerWireframeHighlightSi.h
    piPrototypeWireframeHighlightSi.h
    niInstanceWireframeHighlightSi.h
    niPrototypeWireframeHighlightSi.h
    baseWhSi.h
    #meshWhSi.h
    piInstancerWhSi.h
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
