# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
    fvpColorChanged.cpp
    fvpColorPreferences.cpp
    fvpColorPreferencesTokens.cpp
)

set(HEADERS
    fvpColorChanged.h
    fvpColorPreferences.h
    fvpColorPreferencesTokens.h
    fvpColorPreferencesTranslator.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/colorPreferences
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}/include/flowViewport/colorPreferences
)
