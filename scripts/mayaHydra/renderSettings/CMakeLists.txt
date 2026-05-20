set(PYTHON_FILES
    __init__.py
    camera.py
    crop.py
    output.py
    renderProducts.py
    resolution.py
)

install(
    FILES ${PYTHON_FILES}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/scripts/mayaHydra/renderSettings
)
