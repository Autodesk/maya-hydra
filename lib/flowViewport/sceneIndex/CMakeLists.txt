# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpIsolateSelectSceneIndex.cpp
    fvpMergingSceneIndex.cpp
    fvpPathInterface.cpp
    fvpPathInterfaceSceneIndex.cpp
    fvpRenderIndexProxy.cpp
    fvpSceneIndexUtils.cpp
    fvpSelectionSceneIndex.cpp
    fvpWireframeSelectionHighlightSceneIndex.cpp
    fvpDisplayStyleOverrideSceneIndex.cpp
    fvpPruneTexturesSceneIndex.cpp
    fvpPruningSceneIndex.cpp
    fvpBBoxSceneIndex.cpp
    fvpReprSelectorSceneIndex.cpp
    fvpBlockPrimRemovalPropagationSceneIndex.cpp
	fvpDefaultMaterialSceneIndex.cpp
    fvpLightsManagementSceneIndex.cpp
)

set(HEADERS
    fvpIsolateSelectSceneIndex.h
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
    fvpPruningSceneIndex.h
    fvpBBoxSceneIndex.h
    fvpReprSelectorSceneIndex.h
    fvpBlockPrimRemovalPropagationSceneIndex.h
	fvpDefaultMaterialSceneIndex.h
    fvpLightsManagementSceneIndex.h
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
