# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpMergingSceneIndex.cpp
    fvpPathInterface.cpp
    fvpPathInterfaceSceneIndex.cpp
    fvpRenderIndexProxy.cpp
    fvpSelectionSceneIndex.cpp
    fvpWireframeSelectionHighlightSceneIndex.cpp
    fvpParentDataModifierSceneIndex.cpp
)

set(HEADERS
    fvpMergingSceneIndex.h
    fvpPathInterface.h
    fvpPathInterfaceSceneIndex.h
    fvpRenderIndexProxy.h
    fvpRenderIndexProxyFwd.h
    fvpSelectionSceneIndex.h
    fvpWireframeSelectionHighlightSceneIndex.h
    fvpParentDataModifierSceneIndex.h
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
