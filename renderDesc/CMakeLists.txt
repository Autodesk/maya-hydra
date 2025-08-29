set(RENDER_DESC_FILES
    hydraArnoldRenderer.xml
    hydraPrmanRenderer.xml
    hydraStormRenderer.xml
)

install(
    FILES ${RENDER_DESC_FILES}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/renderDesc
)
