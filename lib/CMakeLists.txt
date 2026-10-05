add_subdirectory(mayaHydra)
add_subdirectory(flowViewport)

# HVT setup
# Use vanilla OpenUSD
set(ENABLE_ADSK_OPENUSD_PENDING OFF)
set(OPENUSD_INSTALL_PATH "${PXR_USD_LOCATION}")
set(ENABLE_DRAW_ORDER OFF)
# Disable tests for HVT.
set(ENABLE_TESTS OFF)
# Disable the HVT rendering test framework (on by default since HVT a7f5a89): Maya Hydra does not
# use it, and it needs SDL2, GLEW and GTest packages that this build does not provide.
set(HVT_BUILD_TEST_FRAMEWORK OFF)
# Disable HVT codesign, we have our own.
set(HVT_CODESIGN OFF)
# Trim windows.h for HVT with every Windows compiler: HVT defines these for MSVC only. With clang++
# (the Coverage variant), the full windows.h defines interface as struct through combaseapi.h,
# which breaks USD's HdMaterialNetworkSchemaTokens once HVT's precompiled header pulls windows.h
# in through oneTBB. Set here, just before add_subdirectory, so it applies to HVT only.
if (WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
add_subdirectory(hydra-viewport-toolbox)
