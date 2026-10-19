add_subdirectory(mayaHydra)
add_subdirectory(flowViewport)

if(MAYAHYDRA_BUILD_HVT)
    # HVT setup
    # Use vanilla OpenUSD
    set(ENABLE_ADSK_OPENUSD_PENDING OFF)
    set(OPENUSD_INSTALL_PATH "${PXR_USD_LOCATION}")
    set(ENABLE_DRAW_ORDER OFF)
    # Disable tests for HVT.
    set(ENABLE_TESTS OFF)
    # Disable the HVT rendering test framework
    set(HVT_BUILD_TEST_FRAMEWORK OFF)
    # Disable HVT codesign, we have our own.
    set(HVT_CODESIGN OFF)
    add_subdirectory(hydra-viewport-toolbox)
else()
    # mayaHydra loads the HVT shaders from <install>/include/hvt/resources (see renderOverride.cpp).
    # The bundled HVT installs them there; an external package keeps them in its own prefix.
    install(DIRECTORY "${VIEWPORTTOOLBOX_RESOURCES_DIR}"
            DESTINATION ${CMAKE_INSTALL_PREFIX}/include/hvt)
endif()
