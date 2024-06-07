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
    fvpPruneTexturesSceneIndex.cpp
    fvpBBoxSceneIndex.cpp
    fvpReprSelectorSceneIndex.cpp
    fvpBlockPrimRemovalPropagationSceneIndex.cpp
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
    fvpPruneTexturesSceneIndex.h
    fvpBBoxSceneIndex.h
    fvpReprSelectorSceneIndex.h
    fvpBlockPrimRemovalPropagationSceneIndex.h
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
