add_subdirectory(mayaHydra)
add_subdirectory(flowViewport)

if (ENABLE_VIEWPORT_TOOLBOX)
    # Use vanilla OpenUSD
    set(ENABLE_ADSK_OPENUSD_PENDING OFF)
    set(OPENUSD_INSTALL_PATH "${PXR_USD_LOCATION}")

    set(ENABLE_DRAW_ORDER OFF)

    # Disable tests for HVT.
    set(ENABLE_TESTS OFF)

    add_subdirectory(hydra-viewport-toolbox)
endif()
