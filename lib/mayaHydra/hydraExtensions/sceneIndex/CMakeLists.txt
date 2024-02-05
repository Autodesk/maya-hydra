# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    registration.cpp
    mayaHydraSceneIndex.cpp
    mayaHydraDataSource.cpp
    mayaHydraPrimvarDataSource.cpp
    mayaHydraDisplayStyleDataSource.cpp
    mayaHydraCameraDataSource.cpp
    mayaHydraLightDataSource.cpp
    mayaHydraDefaultLightDataSource.cpp
    mayaHydraMayaDataProducerSceneIndexData.cpp
    mayaHydraMayaDataProducerSceneIndexDataConcreteFactory.cpp
    mayaHydraSceneIndexDataFactoriesSetup.cpp
    mayaHydraMayaFilteringSceneIndexData.cpp
    mayaHydraMayaFilteringSceneIndexDataConcreteFactory.cpp
    mhMayaUsdProxyShapeSceneIndex.cpp
)

set(HEADERS
    registration.h
    mayaHydraSceneIndex.h
    mayaHydraDataSource.h
    mayaHydraPrimvarDataSource.h
    mayaHydraDisplayStyleDataSource.h
    mayaHydraCameraDataSource.h
    mayaHydraLightDataSource.h
    mayaHydraDefaultLightDataSource.h
    mayaHydraMayaDataProducerSceneIndexData.h
    mayaHydraMayaDataProducerSceneIndexDataConcreteFactory.h
    mayaHydraSceneIndexDataFactoriesSetup.h
    mayaHydraMayaFilteringSceneIndexData.h
    mayaHydraMayaFilteringSceneIndexDataConcreteFactory.h
    mhMayaUsdProxyShapeSceneIndex.h
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
        ${CMAKE_INSTALL_PREFIX}/include/mayaHydraLib/sceneIndex
)
