# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpImageBufferWriter.cpp
    fvpTextureBufferWriter.cpp
    fvpRenderBufferWriter.cpp
    fvpImageBufferWriterFactory.cpp
)

set(HEADERS
    fvpImageBufferWriter.h
    fvpTextureBufferWriter.h
    fvpRenderBufferWriter.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/imageWriter
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/flowViewport/imageWriter
)
