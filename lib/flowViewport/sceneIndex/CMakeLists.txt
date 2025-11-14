# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpIsolateSelectSceneIndex.cpp
    fvpSceneIndexUtils.cpp
    fvpSelectionSceneIndex.cpp
    fvpDisplayStyleOverrideSceneIndex.cpp
    fvpPruneTexturesSceneIndex.cpp
    fvpPruningSceneIndex.cpp
    fvpBBoxSceneIndex.cpp
    fvpReprSelectorSceneIndex.cpp
    fvpBlockPrimRemovalPropagationSceneIndex.cpp
    fvpDefaultMaterialSceneIndex.cpp
    fvpLightsManagementSceneIndex.cpp
    fvpPassFilteringSceneIndex.cpp
    fvpPurposeFilteringSceneIndex.cpp
    fvpDataProducerMergingSceneIndexProxy.cpp
)

set(HEADERS
    fvpIsolateSelectSceneIndex.h
    fvpSceneIndexUtils.h
    fvpSelectionSceneIndex.h
    fvpDisplayStyleOverrideSceneIndex.h
    fvpPruneTexturesSceneIndex.h
    fvpPruningSceneIndex.h
    fvpBBoxSceneIndex.h
    fvpReprSelectorSceneIndex.h
    fvpBlockPrimRemovalPropagationSceneIndex.h
    fvpDefaultMaterialSceneIndex.h
    fvpLightsManagementSceneIndex.h
    fvpPassFilteringSceneIndex.h
    fvpPurposeFilteringSceneIndex.h
    fvpDataProducerMergingSceneIndexProxy.h
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

add_subdirectory(wireframeHighlights)
