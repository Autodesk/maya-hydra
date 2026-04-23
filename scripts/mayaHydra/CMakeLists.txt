add_subdirectory(renderSettings)

set(PYTHON_FILES
    __init__.py
)

install(
    FILES ${PYTHON_FILES}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/scripts/mayaHydra
)
