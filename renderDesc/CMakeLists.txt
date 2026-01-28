set(RENDER_DESC_FILES
    HdArnoldRendererPluginRenderer.xml
    HdPrmanLoaderRendererPluginRenderer.xml
    HdStormRendererPluginRenderer.xml
)

install(
    FILES ${RENDER_DESC_FILES}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/renderDesc
)
