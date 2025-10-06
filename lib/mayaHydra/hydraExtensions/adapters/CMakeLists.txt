# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
        adapter.cpp
        adapterDebugCodes.cpp
        adapterRegistry.cpp
        aiSkydomeLightAdapter.cpp
		aiAreaLightAdapter.cpp
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
		shadowMatrixComputation.cpp
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
	shadowMatrixComputation.h
)

if (MAYA_HAS_RENDER_ITEM_CULL_MODE_API)
    target_compile_definitions(${TARGET_NAME}
        PRIVATE
            MAYA_HAS_RENDER_ITEM_CULL_MODE_API=1
    )
endif()

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
