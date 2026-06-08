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
    cmake_path(GET scene STEM sceneFileNameNoExt)
    mayaHydra_add_cmd_line_render_test(${scene}
        COPY_SCENE
        RENDERED_IMAGE_SUBDIR projects/default/images
        RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
        RENDERER_ARGS "-x 960 -y 540 -rd ../images -im ${sceneFileNameNoExt}"
    )
endforeach()

foreach(scene ${ARNOLD_RENDER_SCENES})
    cmake_path(GET scene STEM sceneFileNameNoExt)
    mayaHydra_add_cmd_line_render_test(${scene}
        COPY_SCENE
        RENDERER HdArnoldRendererPlugin
        RENDERED_IMAGE_SUBDIR projects/default/images
        RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
        RENDERER_ARGS "-x 160 -y 120 -rd ../images -im ${sceneFileNameNoExt}"
    )
endforeach()

mayaHydra_add_cmd_line_render_test(renderSettings/stormResolution.ma
    COPY_SCENE
    RENDERER HdStormRendererPlugin
    RENDERER_ARGS "-x 720 -y 480"
)

# Test a render using the fallback USD resolution settings
set(scene basic/sphereFallbackResolution.ma)
cmake_path(GET scene STEM sceneFileNameNoExt)
mayaHydra_add_cmd_line_render_test(${scene}
    COPY_SCENE
    RENDERED_IMAGE_SUBDIR projects/default/images
    RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
    RENDERER HdStormRendererPlugin
    RENDERER_ARGS "-rd ../images -im ${sceneFileNameNoExt}"
)

# Test a render using a different camera than that from the fallback
# USD resolution settings: Maya camera.
set(scene basic/mayaCamera.ma)
cmake_path(GET scene STEM sceneFileNameNoExt)
mayaHydra_add_cmd_line_render_test(${scene}
    COPY_SCENE
    RENDERED_IMAGE_SUBDIR projects/default/images
    RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
    RENDERER HdStormRendererPlugin
    RENDERER_ARGS "-x 960 -y 540 -cam \"|camera1\" -rd ../images -im ${sceneFileNameNoExt}"
)

# Test a render using a different camera than that from the fallback
# USD resolution settings: USD camera.
set(scene basic/usdCamera.ma)
cmake_path(GET scene STEM sceneFileNameNoExt)
mayaHydra_add_cmd_line_render_test(${scene}
    COPY_SCENE
    RENDERED_IMAGE_SUBDIR projects/default/images
    RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
    RENDERER HdStormRendererPlugin
    RENDERER_ARGS "-x 960 -y 540 -cam \"|stage1|stageShape1,/camera1\" -rd ../images -im ${sceneFileNameNoExt}"
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

# No crop region: full 64x64 reference render of the same scene used by the
# stormCropRegion / stormCropRegion1x1 tests.  Acts as the uncropped
# baseline so a regression that breaks `-reg` would still need to keep this
# test green (i.e. cropped and uncropped renders cannot accidentally
# converge).
mayaHydra_add_cmd_line_render_test(renderSettings/stormNoCrop.ma
    COPY_SCENE
    RENDERER HdStormRendererPlugin
    RENDERER_ARGS "-x 64 -y 64"
)

# Crop region: render only the inner 32x32 of a 64x64 image.  Bounds are
# inclusive on all four sides, so [16, 47] x [16, 47] selects 32 pixels.
mayaHydra_add_cmd_line_render_test(renderSettings/stormCropRegion.ma
    COPY_SCENE
    RENDERER HdStormRendererPlugin
    RENDERER_ARGS "-x 64 -y 64 -reg 16 47 16 47"
)

# Single-pixel crop region: only pixel (32, 32) is rendered, every other
# pixel is the AOV clear/black value.  Exercises the inclusive-bound math
# in mayaHydra.renderSettings.crop._toDataWindowNDC for a 1x1 region.
mayaHydra_add_cmd_line_render_test(renderSettings/stormCropRegion1x1.ma
    COPY_SCENE
    RENDERER HdStormRendererPlugin
    RENDERER_ARGS "-x 64 -y 64 -reg 32 32 32 32"
)

# Arnold crop region without USD render settings: exercises the Maya render
# settings fallback path, where -reg is carried through mayaHydraRenderRegion
# instead of UsdRenderProduct.dataWindowNDC.
#
# Disabled, needs USD render settings, entered as HYDRA-2325
#
# mayaHydra_add_cmd_line_render_test(renderSettings/arnoldCropRegion.ma
#     RENDERER HdArnoldRendererPlugin
#     RENDERER_ARGS "-x 160 -y 120 -reg 32 95 30 89"
#     FAIL 0.5
# )

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

# Test frame range (5-9) with HdStorm (Failure with HdStorm on macOS entered as HYDRA-2139.)
if(NOT IS_MACOSX)
mayaHydra_add_cmd_line_render_multi_image_test(renderSettings/AnimCubeRenderSettings.ma
    EXPECTED_IMAGES_DIR renderSettings/images/storm_frames5to9
    IMAGE_EXTENSION png
    TEST_NAME_SUFFIX StormFrames5to9
    RENDERER_ARGS "-s 5 -e 9"
)
endif()

# Test frame range (5-9) with HdArnold
#
# Disabled, reference images have watermark, entered as HYDRA-2325
#
# mayaHydra_add_cmd_line_render_multi_image_test(renderSettings/AnimCubeRenderSettings.ma
#     RENDERER HdArnoldRendererPlugin
#     EXPECTED_IMAGES_DIR renderSettings/images/arnold_frames5to9
#     IMAGE_EXTENSION png
#     TEST_NAME_SUFFIX ArnoldFrames5to9
#     FAIL 0.05
#     FAILPERCENT 1.0
#     RENDERER_ARGS "-s 5 -e 9"
# )

# Stage UVChecker.png within this test’s temporary Maya project so it can be
# resolved via the workspace root when using `-proj`.
#
# Expected test directory layout:
# .../test/Temporary/mayaSphereMayaMaterialArnoldRelativePath/projects/default   ← what -proj points at
# ├── images/mayaSphereMayaMaterialArnoldRelativePath.png
# └── sourceImages/UVChecker.png
set(_relpath_proj_dir
    "${CMAKE_BINARY_DIR}/test/Temporary/mayaSphereMayaMaterialArnoldRelativePath/projects/default")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/scenes/texturing/UVChecker.png"
    "${_relpath_proj_dir}/sourceImages/UVChecker.png"
    COPYONLY
)

# Test relative path resolution for Maya scene files.
# Scenes may be transferred across different environments (e.g., render farms),
# so the `-proj` argument is used to define the project root for resolving asset paths.
# This ensures that relative paths are correctly remapped at load time.
#
# Example of an absolute path embedded in a .ma file:
#   setAttr ".ftn" -type "string" "D:/nonExistentPath//UVChecker.png";
# The test verifies that such paths can be resolved relative to the specified project.
set(render_dir ${CMAKE_BINARY_DIR}/test/Temporary/mayaSphereMayaMaterialArnoldRelativePath/projects/default/images)
set(scene relativePathTest/mayaSphereMayaMaterialArnoldRelativePath.ma)
cmake_path(GET scene STEM sceneFileNameNoExt)
mayaHydra_add_cmd_line_render_test(${scene}
    RENDERER HdArnoldRendererPlugin
    # Must specify:
    # - The resolution, otherwise the USD default render settings 2048x1080
    #   will be used
    # - The render directory and image name, otherwise the USD default render
    #   settings ./default.png will be used, which places the rendered
    #   output in the source tree in the Maya scene file directory, with
    #   image name "default.png".
    RENDERER_ARGS "-x 160 -y 120 -proj \"${_relpath_proj_dir}\" -rd \"${render_dir}\" -im ${sceneFileNameNoExt}"
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
