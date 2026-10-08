add_subdirectory(mayaHydra)
add_subdirectory(flowViewport)

# HVT setup
# Use vanilla OpenUSD
set(ENABLE_ADSK_OPENUSD_PENDING OFF)
set(OPENUSD_INSTALL_PATH "${PXR_USD_LOCATION}")
set(ENABLE_DRAW_ORDER OFF)
# Disable tests for HVT.
set(ENABLE_TESTS OFF)
# Disable HVT's rendering test framework: it is unused here and pulls in SDL2/GLEW/GTest, which
# this build does not provide.
set(HVT_BUILD_TEST_FRAMEWORK OFF)
# Disable HVT codesign, we have our own.
set(HVT_CODESIGN OFF)
add_subdirectory(hydra-viewport-toolbox)
