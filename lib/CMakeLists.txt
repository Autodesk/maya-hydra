add_subdirectory(mayaHydra)
add_subdirectory(flowViewport)

# HVT setup
# Use vanilla OpenUSD
set(ENABLE_ADSK_OPENUSD_PENDING OFF)
set(OPENUSD_INSTALL_PATH "${PXR_USD_LOCATION}")
set(ENABLE_DRAW_ORDER OFF)
# Disable tests for HVT.
set(ENABLE_TESTS OFF)
add_subdirectory(hydra-viewport-toolbox)

# FIXME HYDRA-2275 : Demote -Wmaybe-uninitialized from error to warning for HVT targets.
# Some Linux machines/compilers are stricter than others about this.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    foreach(_hvt_target hvt hvt_engine hvt_dataSource hvt_geometry hvt_sceneIndex hvt_shadow hvt_tasks hvt_utils)
        if(TARGET ${_hvt_target})
            target_compile_options(${_hvt_target} PRIVATE -Wno-error=maybe-uninitialized)
        endif()
    endforeach()
endif()
