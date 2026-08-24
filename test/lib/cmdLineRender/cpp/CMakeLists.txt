set(_maya_hydra_cpp_tests_target mayaHydraCppTests)

if(NOT TARGET ${_maya_hydra_cpp_tests_target})
    return()
endif()

# testUtils.h lives in mayaToHydra/cpp.
set(_maya_hydra_cpp_test_utils_dir
    "${CMAKE_CURRENT_SOURCE_DIR}/../../mayaUsd/render/mayaToHydra/cpp")

target_sources(${_maya_hydra_cpp_tests_target}
    PRIVATE
        testRenderingColorSpaceResolving.cpp
)

target_include_directories(${_maya_hydra_cpp_tests_target}
    PRIVATE
        ${_maya_hydra_cpp_test_utils_dir}
)
