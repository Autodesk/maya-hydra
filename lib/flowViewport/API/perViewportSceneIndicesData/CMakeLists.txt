# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpViewportInformationAndSceneIndicesPerViewportData.cpp
    fvpViewportInformationAndSceneIndicesPerViewportDataManager.cpp
    fvpDataProducerSceneIndexDataBase.cpp
    fvpFilteringSceneIndexDataBase.cpp
    fvpFilteringSceneIndicesChainManager.cpp
    fvpViewportAPITokens.cpp
)

set(HEADERS
    fvpViewportInformationAndSceneIndicesPerViewportData.h
    fvpViewportInformationAndSceneIndicesPerViewportDataManager.h
    fvpDataProducerSceneIndexDataAbstractFactory.h
    fvpDataProducerSceneIndexDataBase.h
    fvpFilteringSceneIndexDataAbstractFactory.h
    fvpFilteringSceneIndexDataBase.h
    fvpFilteringSceneIndicesChainManager.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/API/perViewportSceneIndicesData
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/flowViewport/API/perViewportSceneIndicesData
)
