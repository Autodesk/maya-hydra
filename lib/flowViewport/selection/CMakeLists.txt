# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpSelection.cpp
    fvpSelectionFwd.cpp
    fvpSelectionTask.cpp
    fvpSelectionTracker.cpp
    fvpSelectionTypes.cpp
    fvpPathMapper.cpp
    fvpPathMapperFwd.cpp
    fvpPathMapperRegistry.cpp
    fvpPrefixPathMapper.cpp
    fvpDataProducersNodeHashCodeToSdfPathRegistry.cpp
)

set(HEADERS
    fvpSelection.h
    fvpSelectionFwd.h
    fvpSelectionTask.h
    fvpSelectionTracker.h
    fvpSelectionTypes.h
    fvpPathMapper.h
    fvpPathMapperFwd.h
    fvpPathMapperRegistry.h
    fvpPrefixPathMapper.h
    fvpDataProducersNodeHashCodeToSdfPathRegistry.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/selection
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/flowViewport/selection
)
