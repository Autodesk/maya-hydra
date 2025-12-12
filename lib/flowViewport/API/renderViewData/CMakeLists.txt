# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpRenderViewData.cpp
    fvpRenderViewDataManager.cpp
    fvpDataProducerSceneIndexDataBase.cpp
    fvpFilteringSceneIndexDataBase.cpp
    fvpFilteringSceneIndicesChainManager.cpp
    fvpIsolateSelectManager.cpp
    fvpViewportAPITokens.cpp
)

set(HEADERS
    fvpRenderViewData.h
    fvpRenderViewDataManager.h
    fvpDataProducerSceneIndexDataAbstractFactory.h
    fvpDataProducerSceneIndexDataBase.h
    fvpFilteringSceneIndexDataAbstractFactory.h
    fvpFilteringSceneIndexDataBase.h
    fvpFilteringSceneIndicesChainManager.h
    fvpIsolateSelectManager.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/API/renderViewData
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/flowViewport/API/renderViewData
)
