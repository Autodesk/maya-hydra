# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpMergingSceneIndex.cpp
    fvpPathInterface.cpp
    fvpPathInterfaceSceneIndex.cpp
    fvpRenderIndexProxy.cpp
    fvpSceneIndexUtils.cpp
    fvpSelectionSceneIndex.cpp
    fvpWireframeSelectionHighlightSceneIndex.cpp
    fvpDisplayStyleOverrideSceneIndex.cpp
    fvpBBoxSceneIndex.cpp
)

set(HEADERS
    fvpMergingSceneIndex.h
    fvpPathInterface.h
    fvpPathInterfaceSceneIndex.h
    fvpRenderIndexProxy.h
    fvpRenderIndexProxyFwd.h
    fvpSceneIndexUtils.h
    fvpSelectionSceneIndex.h
    fvpWireframeSelectionHighlightSceneIndex.h
    fvpDisplayStyleOverrideSceneIndex.h
    fvpBBoxSceneIndex.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/sceneIndex
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/flowViewport/sceneIndex
)
