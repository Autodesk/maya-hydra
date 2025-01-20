# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    baseWhSi.cpp
    geomSubsetWhSi.cpp
    meshWhSi.cpp
    niInstanceWhSi.cpp
    niPrototypeWhSi.cpp
    piInstancerWhSi.cpp
    piPrototypeWhSi.cpp
)

set(HEADERS
    baseWhSi.h
    geomSubsetWhSi.h
    meshWhSi.h
    niInstanceWhSi.h
    niPrototypeWhSi.h
    piInstancerWhSi.h
    piPrototypeWhSi.h
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
