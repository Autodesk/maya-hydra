
set(SHADERS_FILES
    customBasicLightingShader.glslfx
    customBufferShader.glslfx
	shaderDefs.usda
    myCustomShader.ogsfx
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------

install(FILES ${SHADERS_FILES}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/lib/usd/flowViewportShadersDiscoveryPlugin/shaders
)
