# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
        snapshotEnvSettings.cpp
        snapshotPublishBarrier.cpp
)

set(HEADERS
    snapshotEnvSettings.h
    snapshotPublishBarrier.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList(
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/snapshot
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION
        ${CMAKE_INSTALL_PREFIX}/include/mayaHydraLib/snapshot
)
