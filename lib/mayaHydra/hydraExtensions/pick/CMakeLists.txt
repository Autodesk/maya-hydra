# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    mhPickContext.cpp
    mhPickContextFwd.cpp
    mhPickHandler.cpp
    mhPickHandlerFwd.cpp
    mhPickHandlerRegistry.cpp
    mhUsdPickHandler.cpp
    mhPickHit.cpp
    mhPickHitFwd.cpp
)

set(HEADERS
    mhPickContext.h
    mhPickContextFwd.h
    mhPickHandler.h
    mhPickHandlerFwd.h
    mhPickHandlerRegistry.h
    mhUsdPickHandler.h
    mhPickHit.h
    mhPickHitFwd.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/pick
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/mayaHydraLib/pick
)
