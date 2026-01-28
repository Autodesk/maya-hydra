# Command line render unit test scripts, launched with Render.exe.
# Unit test can be disabled with ctest labels.
# Adding labels to the unit test follows this convention:
# myTestFile.ma|depOnPlugins:lookdev,mtoa,newPlugin|skipOnPlatform:[osx,win,lin]
# See accompanying README.md for currently supported list of label mapping.
# only single platform exclusion is supported currently.
# For specifying exclusion based on plugin dependency, look at: test_to_run.json
if(IS_WINDOWS)
set(STORM_RENDER_SCENES
    basic/sphere.ma
    texturing/mayaSphereMayaMaterialStorm.ma
# HYDRA-2076
# Disabled because loaded MaterialX file cannot be relative to Maya file.
#   texturing/mayaSphereMaterialXStorm.ma
    texturing/usdSphereMaterialXStorm.ma
    texturing/usdSphereUsdMaterialStorm.ma
)

set(ARNOLD_RENDER_SCENES
    texturing/mayaSphereMayaMaterialArnold.ma
# HYDRA-2076
# Disabled because loaded MaterialX file cannot be relative to Maya file.
#   texturing/mayaSphereMaterialXArnold.ma
    texturing/usdSphereMaterialXArnold.ma
    texturing/usdSphereUsdMaterialArnold.ma
)
else()
# HYDRA-2031 cmdLineRender tests fail on macOS and Linux.
set(STORM_RENDER_SCENES "")
set(ARNOLD_RENDER_SCENES "")
endif()

foreach(scene ${STORM_RENDER_SCENES})
    mayaHydra_add_cmd_line_render_test(${scene})
endforeach()

foreach(scene ${ARNOLD_RENDER_SCENES})
    mayaHydra_add_cmd_line_render_test(${scene}
    RENDERER HdArnoldRendererPlugin
)
endforeach()

if(NOT IS_MACOSX)
# HYDRA-2031 this test fails only on macOS.
add_test(
  NAME registeredRenderRenderers
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/registeredRenderRenderers.py
          ${RENDER_EXECUTABLE}
          -listRenderers
          # As per Maya source code, the expected exit code for Render
          # when using the -listRenderers argument is 200.
          200
)

set(RENDER_DESC_PATH "${CMAKE_INSTALL_PREFIX}/renderDesc")

set_tests_properties(
  registeredRenderRenderers
  PROPERTIES
    LABELS      "cmdLineRender"
    ENVIRONMENT "MAYA_RENDER_DESC_PATH=${RENDER_DESC_PATH}")
endif()

# Production rendering tests that require a Maya executable (mayabatch).
# Skip on Windows for Maya 2026, fails with
#
# import maya.api._OpenMaya_py2
# ImportError: DLL load failed while importing _OpenMaya_py2: The operating system cannot run %1.
#
# As there are no plans to support Hydra production rendering on Maya 2026,
# we will not investigate this problem.

if(IS_WINDOWS AND MAYA_APP_VERSION VERSION_EQUAL 2026)
set(MAYABATCH_TEST_SCRIPT_FILES "")
else()
set(MAYABATCH_TEST_SCRIPT_FILES
    registeredMayaRenderers.py
)
endif()

foreach(script ${MAYABATCH_TEST_SCRIPT_FILES})    
    get_testfile_and_labels(all_labels test_filename ${script})
    list(APPEND all_labels "cmdLineRender")
    mayaUsd_get_unittest_target(target ${test_filename})
    mayaUsd_add_test(${target}
        PYTHON_SCRIPT ${test_filename}
    )

    # Add a ctest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()
