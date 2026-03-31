# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpBaseWhSi.cpp
    fvpGeomSubsetWhSi.cpp
    fvpMeshWhSi.cpp
    fvpNiInstanceWhSi.cpp
    fvpNiPrototypeWhSi.cpp
    fvpPiInstancerWhSi.cpp
    fvpPiPrototypeWhSi.cpp
    fvpGenerativeProceduralWhSi.cpp
    fvpWhSiSceneIndexPlugin.cpp
)

set(HEADERS
    fvpBaseWhSi.h
    fvpGeomSubsetWhSi.h
    fvpMeshWhSi.h
    fvpNiInstanceWhSi.h
    fvpNiPrototypeWhSi.h
    fvpPiInstancerWhSi.h
    fvpPiPrototypeWhSi.h
    fvpGenerativeProceduralWhSi.h
    fvpWhSiSceneIndexPlugin.h
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
