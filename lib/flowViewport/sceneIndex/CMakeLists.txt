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
    fvpDefaultMaterialSceneIndex.cpp
    fvpLightsManagementSceneIndex.cpp
)

if (MAYA_HAS_VIEW_SELECTED_OBJECT_API)
    target_sources(${TARGET_NAME}
        PRIVATE
        fvpIsolateSelectSceneIndex.cpp
    )
endif()

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
    fvpDefaultMaterialSceneIndex.h
    fvpLightsManagementSceneIndex.h
)

if (MAYA_HAS_VIEW_SELECTED_OBJECT_API)
    list(APPEND HEADERS
        fvpIsolateSelectSceneIndex.h
    )
endif()

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
