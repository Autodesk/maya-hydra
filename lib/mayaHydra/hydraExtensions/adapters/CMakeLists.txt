# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
        adapter.cpp
        adapterDebugCodes.cpp
        adapterRegistry.cpp
        aiSkydomeLightAdapter.cpp
        areaLightAdapter.cpp
        cameraAdapter.cpp
        dagAdapter.cpp
        renderItemAdapter.cpp
        directionalLightAdapter.cpp
        lightAdapter.cpp
        materialAdapter.cpp
        materialNetworkConverter.cpp
        mayaAttrs.cpp
        meshAdapter.cpp
        nurbsCurveAdapter.cpp
        pointLightAdapter.cpp
        shapeAdapter.cpp
        spotLightAdapter.cpp
        tokens.cpp
)

set(HEADERS
    adapter.h
    adapterDebugCodes.h
    adapterRegistry.h
    cameraAdapter.h
    constantShadowMatrix.h
    dagAdapter.h
    renderItemAdapter.h
    lightAdapter.h
    materialAdapter.h
    materialNetworkConverter.h
    mayaAttrs.h
    shapeAdapter.h
    tokens.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/adapters
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/mayaHydraLib/adapters
)
