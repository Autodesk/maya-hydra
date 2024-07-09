
set(SHADERS_FILES
    customBasicLightingShader.glslfx
	shaderDefs.usda
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------

install(FILES ${SHADERS_FILES}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/lib/usd/flowViewportShadersDiscoveryPlugin/shaders
)
