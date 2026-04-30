# Copyright 2026 Autodesk, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Command line render unit test scripts, launched with Render.exe.
# Unit test can be disabled with ctest labels.
# Adding labels to the unit test follows this convention:
# myTestFile.ma|depOnPlugins:lookdev,mtoa,newPlugin|skipOnPlatform:[osx,win,lin]
# See accompanying README.md for currently supported list of label mapping.
# only single platform exclusion is supported currently.
# For specifying exclusion based on plugin dependency, look at: test_to_run.json

# TEMPORARY: skip all cmdLineRender tests so Jenkins runs only the test we are
# debugging (HYDRA-1766: testTransformMeshAdapter). REVERT BEFORE MERGE.
return()

if(NOT IS_MACOSX)
    set(STORM_RENDER_SCENES
        basic/sphere.ma
    )
endif()

# HYDRA-2139 image differences on some HdStorm tests on Linux and macOS.
if(IS_WINDOWS)
    list(APPEND STORM_RENDER_SCENES
        texturing/mayaSphereMayaMaterialStorm.ma
        texturing/usdSphereMaterialXStorm.ma
        texturing/usdSphereUsdMaterialStorm.ma
    )
    # Relative paths for MaterialX documents in Maya files added in Maya 2027.
    if(MAYA_APP_VERSION VERSION_GREATER 2026)
        list(APPEND STORM_RENDER_SCENES texturing/mayaSphereMaterialXStorm.ma)
    endif()
endif()

if(NOT IS_MACOSX)
    set(ARNOLD_RENDER_SCENES
        texturing/mayaSphereMayaMaterialArnold.ma
        texturing/usdSphereMaterialXArnold.ma
        texturing/usdSphereUsdMaterialArnold.ma
    )
    # Relative paths for MaterialX documents in Maya files added in Maya 2027.
    if(MAYA_APP_VERSION VERSION_GREATER 2026)
        list(APPEND ARNOLD_RENDER_SCENES texturing/mayaSphereMaterialXArnold.ma)
    endif()
endif()

foreach(scene ${STORM_RENDER_SCENES})
    mayaHydra_add_cmd_line_render_test(${scene})
endforeach()

foreach(scene ${ARNOLD_RENDER_SCENES})
    mayaHydra_add_cmd_line_render_test(${scene}
    RENDERER HdArnoldRendererPlugin
)
endforeach()

mayaHydra_add_cmd_line_render_test(renderSettings/stormResolution.ma
    COPY_SCENE
    RENDERER HdStormRendererPlugin
    RENDERER_ARGS "-x 720 -y 480"
)

# Use a small vertical resolution to avoid the Arnold watermark.
mayaHydra_add_cmd_line_render_test(renderSettings/arnoldResolution.ma
    COPY_SCENE
    RENDERER HdArnoldRendererPlugin
    RENDERER_ARGS "-x 720 -y 128"
)

# Test output overrides
mayaHydra_add_cmd_line_render_test(renderSettings/stormRenderDir.ma
    COPY_SCENE
    RENDERER HdStormRendererPlugin
    RENDERED_IMAGE_SUBDIR projects/default/images/aSubDirectory
    RENDERER_ARGS "-x 720 -y 480 -rd ../images/aSubDirectory"
)

mayaHydra_add_cmd_line_render_test(renderSettings/arnoldRenderDir.ma
    COPY_SCENE
    RENDERER HdArnoldRendererPlugin
    RENDERED_IMAGE_SUBDIR projects/default/images/aSubDirectory
    RENDERER_ARGS "-x 720 -y 128 -rd ../images/aSubDirectory"
)

mayaHydra_add_cmd_line_render_test(renderSettings/stormImageName.ma
    COPY_SCENE
    RENDERER HdStormRendererPlugin
    RENDERED_IMAGE_NAME aNewStormImageName
    RENDERER_ARGS "-x 720 -y 480 -im aNewStormImageName"
)

mayaHydra_add_cmd_line_render_test(renderSettings/arnoldImageName.ma
    COPY_SCENE
    RENDERER HdArnoldRendererPlugin
    RENDERED_IMAGE_NAME aNewArnoldImageName
    RENDERER_ARGS "-x 720 -y 128 -im aNewArnoldImageName"
)

mayaHydra_add_cmd_line_render_test(renderSettings/stormOutputFormat.ma
    COPY_SCENE
    RENDERER HdStormRendererPlugin
    IMAGE_EXTENSION jpg
    RENDERER_ARGS "-x 720 -y 480 -of jpg"
)

mayaHydra_add_cmd_line_render_test(renderSettings/arnoldOutputFormat.ma
    COPY_SCENE
    RENDERER HdArnoldRendererPlugin
    IMAGE_EXTENSION jpg
    RENDERER_ARGS "-x 720 -y 128 -of jpg"
)

# Failure with HdStorm on macOS entered as HYDRA-2139.
if(NOT IS_MACOSX)
    mayaHydra_add_cmd_line_render_multi_image_test(renderSettings/AnimCubeRenderSettings.ma
        EXPECTED_IMAGES_DIR renderSettings/images/storm
        IMAGE_EXTENSION png
        TEST_NAME_SUFFIX Storm
    )
endif()

mayaHydra_add_cmd_line_render_multi_image_test(renderSettings/AnimCubeRenderSettings.ma
    RENDERER HdArnoldRendererPlugin
    EXPECTED_IMAGES_DIR renderSettings/images/arnold
    IMAGE_EXTENSION png
    TEST_NAME_SUFFIX Arnold
    FAIL 0.05
    FAILPERCENT 1.0
)

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

# registeredMayaRenderers fails on Linux and macOS.
if(IS_LINUX
   OR IS_MACOSX
   OR (IS_WINDOWS AND MAYA_APP_VERSION VERSION_EQUAL 2026))
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
        ENV
            "DISPLAY=:0"
    )

    # Add a ctest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()
