# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpSelection.cpp
    fvpSelectionFwd.cpp
    fvpSelectionTask.cpp
    fvpSelectionTracker.cpp
    fvpPathMapper.cpp
    fvpPathMapperFwd.cpp
    fvpPathMapperRegistry.cpp
    fvpPrefixPathMapper.cpp
)

set(HEADERS
    fvpSelection.h
    fvpSelectionFwd.h
    fvpSelectionTask.h
    fvpSelectionTracker.h
    fvpPathMapper.h
    fvpPathMapperFwd.h
    fvpPathMapperRegistry.h
    fvpPrefixPathMapper.h
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
