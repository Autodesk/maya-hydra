set(PYTHON_FILES
    __init__.py
    camera.py
    crop.py
    frames.py
    output.py
    renderProducts.py
    resolution.py
    utils.py
)

install(
    FILES ${PYTHON_FILES}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/scripts/mayaHydra/renderSettings
)
