set(PYTHON_FILES
    __init__.py
    resolution.py
    output.py
)

install(
    FILES ${PYTHON_FILES}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/scripts/mayaHydra/renderSettings
)
