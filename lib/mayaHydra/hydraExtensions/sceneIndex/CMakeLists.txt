# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    registration.cpp
    mayaHydraSceneIndex.cpp
    mayaViewportSceneIndex.cpp
    mayaHydraDataSource.cpp
    mayaHydraCustomNodeDataSource.cpp
    mayaHydraPrimvarDataSource.cpp
    mayaHydraDisplayStyleDataSource.cpp
    mayaHydraCameraDataSource.cpp
    mayaHydraLightDataSource.cpp
    mayaHydraDefaultLightDataSource.cpp
    mayaHydraMaterialDataSource.cpp
    mayaHydraMayaDataProducerSceneIndexData.cpp
    mayaHydraMayaDataProducerSceneIndexDataConcreteFactory.cpp
    mayaHydraSceneIndexDataFactoriesSetup.cpp
    mayaHydraMayaFilteringSceneIndexData.cpp
    mayaHydraMayaFilteringSceneIndexDataConcreteFactory.cpp
    mhMayaUsdProxyShapeSceneIndexBase.cpp
    mhMayaUsdProxyShapeSceneIndex.cpp
    mhDirtyLeadObjectSceneIndex.cpp
    mhExternalCameraOverrideSceneIndex.cpp
    mhExternalCameraResolvingSceneIndex.cpp
    mhGenerativeProceduralResolvingSceneIndex.cpp
)

set(HEADERS
    registration.h
    mayaHydraSceneIndex.h
    mayaViewportSceneIndex.h
    mayaHydraDataSource.h
    mayaHydraCustomNodeDataSource.h
    mayaHydraPrimvarDataSource.h
    mayaHydraDisplayStyleDataSource.h
    mayaHydraCameraDataSource.h
    mayaHydraLightDataSource.h
    mayaHydraSceneIndexUtils.h
    mayaHydraDefaultLightDataSource.h
    mayaHydraMaterialDataSource.h
    mayaHydraMayaDataProducerSceneIndexData.h
    mayaHydraMayaDataProducerSceneIndexDataConcreteFactory.h
    mayaHydraSceneIndexDataFactoriesSetup.h
    mayaHydraMayaFilteringSceneIndexData.h
    mayaHydraMayaFilteringSceneIndexDataConcreteFactory.h
    mhMayaUsdProxyShapeSceneIndexBase.h
    mhMayaUsdProxyShapeSceneIndex.h
    mhDirtyLeadObjectSceneIndex.h
    mhExternalCameraOverrideSceneIndex.h
    mhExternalCameraResolvingSceneIndex.h
    mhGenerativeProceduralResolvingSceneIndex.h
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
