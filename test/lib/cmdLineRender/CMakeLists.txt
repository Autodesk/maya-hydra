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

set(TEST_OUTPUT_ROOT ${CMAKE_BINARY_DIR}/test/Temporary)

# The command line render tests come in per-renderer variants that share the
# same body but differ in a couple of arguments (resolution, image name).  Each
# variant is emitted by calling a small _mayaHydra_add_*_variant() function once
# per renderer, guarded by the platform conditional that variant needs.  The two
# helpers below derive everything else from the renderer "folder" name so the
# call sites only pass what genuinely varies.

# Map a renderer folder name (storm/arnold/prman) to its Hydra plugin and the
# CamelCase suffix used in scene file stems and test names.  This is an explicit
# map rather than string(TOUPPER ...) because PRMan's suffix is irregular
# (PRMan, not Prman).
function(_mayaHydra_renderer_for_folder folder out_renderer out_suffix)
    if(folder STREQUAL "storm")
        set(${out_renderer} HdStormRendererPlugin PARENT_SCOPE)
        set(${out_suffix} Storm PARENT_SCOPE)
    elseif(folder STREQUAL "arnold")
        set(${out_renderer} HdArnoldRendererPlugin PARENT_SCOPE)
        set(${out_suffix} Arnold PARENT_SCOPE)
    elseif(folder STREQUAL "prman")
        set(${out_renderer} HdPrmanLoaderRendererPlugin PARENT_SCOPE)
        set(${out_suffix} PRMan PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unknown renderer folder: ${folder}")
    endif()
endfunction()

# Tag prman tests with a ctest label ("prman") so a pipeline can select or
# deselect the whole PRMan group in one place via tests-to-run.json, which
# build.py turns into "ctest -L prman" / "-LE prman".  This is a group-selection
# handle, not a check for whether the plugin is installed (PRMan is expected to
# be present on the Windows preflight).  The other renderers are a baseline part
# of the suite and never need to be selected as a group, so they get an empty
# label.  Deriving the label here with a single conditional (instead of carrying
# an extra column through parallel lists) is what lets us avoid the "CMake
# list() drops empty elements" placeholder.
function(_mayaHydra_dep_label_for_folder folder out_label)
    if(folder STREQUAL "prman")
        set(${out_label} "|depOnPlugins:prman" PARENT_SCOPE)
    else()
        set(${out_label} "" PARENT_SCOPE)
    endif()
endfunction()

# _mayaHydra_add_hydra_v2_cmd_line_render_test() -- wraps
# mayaHydra_add_cmd_line_render_test() with the absolute -rd workaround
# needed for tests running under Hydra V2 render settings -- now lives in
# cmake/test.cmake alongside mayaHydra_add_cmd_line_render_test() and
# _mayaHydra_append_prman_production_render_env(), since it is a general
# cmdLineRender test-authoring primitive, not specific to this directory.

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

# Texturing production tests. Scene + baseline live under texturing/<folder>/.
set(_tex_scene_stems
    mayaSphereMayaMaterial
    mayaSphereMaterialX
    usdSphereMaterialX
    usdSphereUsdMaterial
)

function(_mayaHydra_add_texturing_variant folder res_arg)
    _mayaHydra_renderer_for_folder(${folder} renderer suffix)
    _mayaHydra_dep_label_for_folder(${folder} dep_label)

    foreach(stem ${_tex_scene_stems})
        set(scene "texturing/${folder}/${stem}${suffix}.ma")
        cmake_path(GET scene STEM sceneFileNameNoExt)
        if(folder STREQUAL "prman")
            _mayaHydra_add_hydra_v2_cmd_line_render_test(${scene}${dep_label}
                COPY_SCENE
                RENDERER ${renderer}
                OUTPUT_NAME ${sceneFileNameNoExt}
                RENDERED_IMAGE_SUBDIR projects/default/images
                RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
                RENDERER_ARGS "${res_arg} -im ${sceneFileNameNoExt}"
            )
        else()
            mayaHydra_add_cmd_line_render_test(${scene}${dep_label}
                COPY_SCENE
                RENDERER ${renderer}
                RENDERED_IMAGE_SUBDIR projects/default/images
                RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
                RENDERER_ARGS "${res_arg} -rd ../images -im ${sceneFileNameNoExt}"
            )
        endif()
    endforeach()
endfunction()

if(IS_WINDOWS)
    _mayaHydra_add_texturing_variant(storm "-x 960 -y 540")
endif()
if(NOT IS_MACOSX)
    _mayaHydra_add_texturing_variant(arnold "-x 160 -y 120")
endif()
if(IS_WINDOWS)
    _mayaHydra_add_texturing_variant(prman "-x 160 -y 120")
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

if(NOT IS_MACOSX)
    foreach(_tex_stem ${_tex_scene_stems})
        set(scene "texturing/arnold/${_tex_stem}Arnold.ma")
        cmake_path(GET scene STEM sceneFileNameNoExt)
        _mayaHydra_add_hydra_v2_cmd_line_render_test(${scene}
            COPY_SCENE
            RENDERER HdArnoldRendererPlugin
            OUTPUT_NAME ${sceneFileNameNoExt}_v2_rs
            RENDERED_IMAGE_SUBDIR projects/default/images
            RENDERED_IMAGE_NAME ${sceneFileNameNoExt}_v2_rs
            RENDERER_ARGS "-x 160 -y 120 -im ${sceneFileNameNoExt}_v2_rs"
            TEST_NAME_SUFFIX v2_rs
            ENV
                "MAYA_HYDRA_HD_ARNOLD_HYDRA_V2_RENDER_SETTINGS=1"
                "USDIMAGINGGL_ENGINE_ENABLE_SCENE_INDEX=1"
                "TF_DEBUG=MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS"
        )
    endforeach()
endif()

# Render settings production tests. Scene + baseline live under renderSettings/<folder>/.
function(_mayaHydra_add_render_settings_variant folder res_arg new_image_name)
    _mayaHydra_renderer_for_folder(${folder} renderer suffix)
    _mayaHydra_dep_label_for_folder(${folder} dep_label)

    set(animcube_scene "renderSettings/${folder}/AnimCubeRenderSettings.ma")
    set(singlecam_scene "renderSettings/${folder}/AnimCubeRenderSettingsSingleCam.ma")

    set(scene "renderSettings/${folder}/${folder}Resolution.ma")
    if(folder STREQUAL "prman")
        _mayaHydra_add_hydra_v2_cmd_line_render_test(${scene}${dep_label}
            COPY_SCENE
            RENDERER ${renderer}
            OUTPUT_NAME ${folder}Resolution
            RENDERER_ARGS "${res_arg} -im ${folder}Resolution"
        )
    else()
        mayaHydra_add_cmd_line_render_test(${scene}${dep_label}
            COPY_SCENE
            RENDERER ${renderer}
            RENDERER_ARGS "${res_arg} -rd ../images -im ${folder}Resolution"
        )
    endif()

    set(scene "renderSettings/${folder}/${folder}RenderDir.ma")
    if(folder STREQUAL "prman")
        _mayaHydra_add_hydra_v2_cmd_line_render_test(${scene}${dep_label}
            COPY_SCENE
            RENDERER ${renderer}
            OUTPUT_NAME ${folder}RenderDir
            RENDERED_IMAGE_SUBDIR projects/default/images/aSubDirectory
            PRECREATE_DIR
            RENDERER_ARGS "${res_arg} -im ${folder}RenderDir"
        )
    else()
        mayaHydra_add_cmd_line_render_test(${scene}${dep_label}
            COPY_SCENE
            RENDERER ${renderer}
            RENDERED_IMAGE_SUBDIR projects/default/images/aSubDirectory
            RENDERER_ARGS "${res_arg} -rd ../images/aSubDirectory"
        )
    endif()

    set(scene "renderSettings/${folder}/${folder}ImageName.ma")
    if(folder STREQUAL "prman")
        _mayaHydra_add_hydra_v2_cmd_line_render_test(${scene}${dep_label}
            COPY_SCENE
            RENDERER ${renderer}
            OUTPUT_NAME ${folder}ImageName
            RENDERED_IMAGE_NAME ${new_image_name}
            RENDERER_ARGS "${res_arg} -im ${new_image_name}"
        )
    else()
        mayaHydra_add_cmd_line_render_test(${scene}${dep_label}
            COPY_SCENE
            RENDERER ${renderer}
            RENDERED_IMAGE_NAME ${new_image_name}
            RENDERER_ARGS "${res_arg} -rd ../images -im ${new_image_name}"
        )
    endif()

    set(scene "renderSettings/${folder}/${folder}OutputFormat.ma")
    if(folder STREQUAL "prman")
        _mayaHydra_add_hydra_v2_cmd_line_render_test(${scene}${dep_label}
            COPY_SCENE
            RENDERER ${renderer}
            OUTPUT_NAME ${folder}OutputFormat
            IMAGE_EXTENSION exr
            RENDERER_ARGS "${res_arg} -im ${folder}OutputFormat -of exr"
        )
    else()
        mayaHydra_add_cmd_line_render_test(${scene}${dep_label}
            COPY_SCENE
            RENDERER ${renderer}
            IMAGE_EXTENSION jpg
            RENDERER_ARGS "${res_arg} -rd ../images -im ${folder}OutputFormat -of jpg"
        )
    endif()

    if(folder STREQUAL "storm")
        # No crop region: full 64x64 reference render of the same scene used by the
        # stormCropRegion / stormCropRegion1x1 tests.  Acts as the uncropped
        # baseline so a regression that breaks `-reg` would still need to keep this
        # test green (i.e. cropped and uncropped renders cannot accidentally
        # converge).
        mayaHydra_add_cmd_line_render_test(renderSettings/storm/stormNoCrop.ma
            COPY_SCENE
            RENDERER HdStormRendererPlugin
            RENDERER_ARGS "-x 64 -y 64 -rd ../images -im stormNoCrop"
        )
        # Crop region: render only the inner 32x32 of a 64x64 image.  Bounds are
        # inclusive on all four sides, so [16, 47] x [16, 47] selects 32 pixels.
        mayaHydra_add_cmd_line_render_test(renderSettings/storm/stormCropRegion.ma
            COPY_SCENE
            RENDERER HdStormRendererPlugin
            RENDERER_ARGS "-x 64 -y 64 -reg 16 47 16 47 -rd ../images -im stormCropRegion"
        )
        # Single-pixel crop region: only pixel (32, 32) is rendered, every other
        # pixel is the AOV clear/black value.  Exercises the inclusive-bound math
        # in mayaHydra.renderSettings.crop._toDataWindowNDC for a 1x1 region.
        mayaHydra_add_cmd_line_render_test(renderSettings/storm/stormCropRegion1x1.ma
            COPY_SCENE
            RENDERER HdStormRendererPlugin
            RENDERER_ARGS "-x 64 -y 64 -reg 32 32 32 32 -rd ../images -im stormCropRegion1x1"
        )
    else()
        set(scene "renderSettings/${folder}/${folder}CropRegion.ma")
        cmake_path(GET scene STEM sceneFileNameNoExt)
        if(folder STREQUAL "prman")
            _mayaHydra_add_hydra_v2_cmd_line_render_test(${scene}${dep_label}
                RENDERER ${renderer}
                OUTPUT_NAME ${folder}CropRegion
                RENDERER_ARGS "-x 160 -y 120 -reg 32 95 30 89 -im ${sceneFileNameNoExt}"
                FAIL 0.5
            )
        else()
            # This test has no COPY_SCENE, so it renders from its original
            # location in the source tree rather than a copied temp project;
            # a relative -rd would land outside the temp dir idiff checks, for
            # a reason unrelated to Hydra V2, so it always uses an absolute
            # path regardless of renderer.
            set(render_dir ${TEST_OUTPUT_ROOT}/${folder}CropRegion/projects/default/images)
            mayaHydra_add_cmd_line_render_test(${scene}${dep_label}
                RENDERER ${renderer}
                RENDERER_ARGS "-x 160 -y 120 -reg 32 95 30 89 -rd \"${render_dir}\" -im ${sceneFileNameNoExt}"
                FAIL 0.5
            )
        endif()
    endif()

    # Failure with HdStorm on macOS entered as HYDRA-2139.
    if(NOT IS_MACOSX OR NOT folder STREQUAL "storm")
        set(multi_fail_args "")
        if(NOT folder STREQUAL "storm")
            set(multi_fail_args FAIL 0.05 FAILPERCENT 1.0)
        endif()
        mayaHydra_add_cmd_line_render_multi_image_test(${animcube_scene}${dep_label}
            RENDERER ${renderer}
            EXPECTED_IMAGES_DIR renderSettings/${folder}/images
            IMAGE_EXTENSION png
            TEST_NAME_SUFFIX ${suffix}
            ${multi_fail_args}
        )
    endif()

    # Test frame range (5-9).  Failure with HdStorm on macOS entered as HYDRA-2139.
    if(folder STREQUAL "storm")
        if(NOT IS_MACOSX)
            mayaHydra_add_cmd_line_render_multi_image_test(${animcube_scene}${dep_label}
                RENDERER ${renderer}
                EXPECTED_IMAGES_DIR renderSettings/${folder}/images/frames5to9
                IMAGE_EXTENSION png
                TEST_NAME_SUFFIX ${suffix}Frames5to9
                RENDERER_ARGS "-s 5 -e 9"
            )
        endif()
    else()
        mayaHydra_add_cmd_line_render_multi_image_test(${animcube_scene}${dep_label}
            RENDERER ${renderer}
            EXPECTED_IMAGES_DIR renderSettings/${folder}/images/frames5to9
            IMAGE_EXTENSION png
            TEST_NAME_SUFFIX ${suffix}Frames5to9
            FAIL 0.05
            FAILPERCENT 1.0
            RENDERER_ARGS "-s 5 -e 9"
        )
    endif()

    set(singlecam_fail_args FAIL 0.05 FAILPERCENT 1.0)
    if(folder STREQUAL "storm")
        set(singlecam_fail_args "")
    endif()
    mayaHydra_add_cmd_line_render_multi_image_test(${singlecam_scene}${dep_label}
        RENDERER ${renderer}
        EXPECTED_IMAGES_DIR renderSettings/${folder}/images/singleCam_frames5to9
        RENDERED_IMAGE_SUBDIR projects/default/images/aSubDirectory
        IMAGE_EXTENSION png
        TEST_NAME_SUFFIX ${suffix}RenderDir
        ${singlecam_fail_args}
        RENDERER_ARGS "-rd images/aSubDirectory"
    )

    # Frame number resolution in render product names (frame 9).  Token expansion
    # in productName requires Hydra v2 render settings (Arnold env or PRMan production
    # render env).  Storm uses Hydra v1 here and does not resolve # tokens, so it is
    # intentionally excluded (the pre-HYDRA-2460 test was Arnold-only for the same reason).
    #
    # As of 2026-Aug-17, HdArnold crashes for multi-frame renders in Hydra v2
    # render settings mode (-s 9 -e 11); single-frame (-s 9 -e 9) is used instead.
    set(_add_frame_nb_resolution FALSE)
    if(folder STREQUAL "arnold")
        set(_add_frame_nb_resolution TRUE)
    elseif(folder STREQUAL "prman" AND IS_WINDOWS)
        set(_add_frame_nb_resolution TRUE)
    endif()

    if(_add_frame_nb_resolution)
        set(frame_nb_scene "renderSettings/${folder}/frameNbResolution.ma")
        if(folder STREQUAL "arnold")
            mayaHydra_add_cmd_line_render_multi_image_test(${frame_nb_scene}${dep_label}
                RENDERER ${renderer}
                EXPECTED_IMAGES_DIR renderSettings/${folder}/images/frameNbResolution
                IMAGE_EXTENSION png
                TEST_NAME_SUFFIX ${suffix}
                FAIL 0.05
                FAILPERCENT 1.0
                # Hydra v2 does not resolve relative render product paths.
                RENDERER_ARGS
                    "-s 9 -e 9 -rd \"${TEST_OUTPUT_ROOT}/frameNbResolution_${suffix}/projects/default/images\""
                ENV
                    "MAYA_HYDRA_HD_ARNOLD_HYDRA_V2_RENDER_SETTINGS=1"
                    "USDIMAGINGGL_ENGINE_ENABLE_SCENE_INDEX=1"
            )
        else()
            mayaHydra_add_cmd_line_render_multi_image_test(${frame_nb_scene}${dep_label}
                RENDERER ${renderer}
                EXPECTED_IMAGES_DIR renderSettings/${folder}/images/frameNbResolution
                IMAGE_EXTENSION png
                TEST_NAME_SUFFIX ${suffix}
                FAIL 0.05
                FAILPERCENT 1.0
                RENDERER_ARGS "-s 9 -e 9"
            )
        endif()
    endif()
endfunction()

_mayaHydra_add_render_settings_variant(storm "-x 720 -y 480" aNewStormImageName)
_mayaHydra_add_render_settings_variant(arnold "-x 720 -y 128" aNewArnoldImageName)
if(IS_WINDOWS)
    _mayaHydra_add_render_settings_variant(prman "-x 720 -y 128" aNewPRManImageName)
endif()

# Test that HdArnold can render a basic render pass.
mayaHydra_add_cmd_line_render_test(renderSettings/arnold/arnoldSimpleRenderPass.ma
    COPY_SCENE
    RENDERER HdArnoldRendererPlugin
    RENDERED_IMAGE_SUBDIR projects/default/images
    RENDERER_ARGS "-x 160 -y 120"
)

set(ARNOLD_MAYABATCH_RENDER_SCENES
    basic/renderCurrentFrame.ma
    basic/renderFrame.ma
)

foreach(scene ${ARNOLD_MAYABATCH_RENDER_SCENES})
    mayaHydra_add_mayabatch_render_test(${scene}
        COPY_SCENE
    )
endforeach()


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
    TEST_NAME_SUFFIX Storm
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
    TEST_NAME_SUFFIX Storm
    RENDERER_ARGS "-x 960 -y 540 -cam \"|stage1|stageShape1,/camera1\" -rd ../images -im ${sceneFileNameNoExt}"
)

# Test -cam with USD camera in HdArnold.
set(scene basic/usdCamera.ma)
cmake_path(GET scene STEM sceneFileNameNoExt)
mayaHydra_add_cmd_line_render_test(${scene}
    COPY_SCENE
    RENDERED_IMAGE_SUBDIR projects/default/images
    RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
    RENDERER HdArnoldRendererPlugin
    TEST_NAME_SUFFIX Arnold
    RENDERER_ARGS "-x 960 -y 540 -cam \"|stage1|stageShape1,/camera1\" -rd ../images -im ${sceneFileNameNoExt}"
)

# Test the same as above but with Hydra V2 render settings.
set(scene basic/usdCamera.ma)
cmake_path(GET scene STEM sceneFileNameNoExt)
_mayaHydra_add_hydra_v2_cmd_line_render_test(${scene}
    COPY_SCENE
    RENDERER HdArnoldRendererPlugin
    OUTPUT_NAME ${sceneFileNameNoExt}_Arnold_v2_rs
    RENDERED_IMAGE_SUBDIR projects/default/images
    RENDERED_IMAGE_NAME ${sceneFileNameNoExt}_Arnold_v2_rs
    RENDERER_ARGS "-x 960 -y 540 -cam \"|stage1|stageShape1,/camera1\" -im ${sceneFileNameNoExt}_Arnold_v2_rs"
    TEST_NAME_SUFFIX Arnold_v2_rs
    ENV
        "MAYA_HYDRA_HD_ARNOLD_HYDRA_V2_RENDER_SETTINGS=1"
        "USDIMAGINGGL_ENGINE_ENABLE_SCENE_INDEX=1"
        "TF_DEBUG=MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS"
)

# PRMan production rendering tests (Windows only).  Hydra V2 render settings and
# isolated RenderMan config are applied automatically in cmake/test.cmake when
# RENDERER is HdPrmanLoaderRendererPlugin.
if(IS_WINDOWS)
    # Test -cam with USD camera in PRMan (Hydra V1 render settings, same path as Storm).
    set(scene basic/usdCamera.ma)
    cmake_path(GET scene STEM sceneFileNameNoExt)
    # PRMan resolves -rd relative to the workspace root, not the copied
    # project/scene layout, so Storm/Arnold's "../images" would write
    # outside the temp dir that idiff checks.
    set(render_dir ${CMAKE_BINARY_DIR}/test/Temporary/${sceneFileNameNoExt}_PRMan/projects/default/images)
    mayaHydra_add_cmd_line_render_test(${scene}|depOnPlugins:prman
        COPY_SCENE
        RENDERER HdPrmanLoaderRendererPlugin
        RENDERED_IMAGE_SUBDIR projects/default/images
        RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
        TEST_NAME_SUFFIX PRMan
        RENDERER_ARGS "-x 960 -y 540 -cam \"|stage1|stageShape1,/camera1\" -rd \"${render_dir}\" -im ${sceneFileNameNoExt}"
    )
    # Override the default Hydra V2 env set by _mayaHydra_append_prman_production_render_env.
    set_property(TEST "usdCamera_PRMan" APPEND PROPERTY ENVIRONMENT
        "HD_PRMAN_RENDER_SETTINGS_DRIVE_RENDER_PASS=")

    # Hydra V2: PRMan reads USD RenderVar prims directly and requires "Ci".
    set(scene basic/usdCamera_PRMan_v2_rs.ma)
    cmake_path(GET scene STEM sceneFileNameNoExt)
    _mayaHydra_add_hydra_v2_cmd_line_render_test(${scene}|depOnPlugins:prman
        COPY_SCENE
        RENDERER HdPrmanLoaderRendererPlugin
        OUTPUT_NAME ${sceneFileNameNoExt}
        RENDERED_IMAGE_SUBDIR projects/default/images
        RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
        RENDERER_ARGS "-x 960 -y 540 -cam \"|stage1|stageShape1,/camera1\" -im ${sceneFileNameNoExt}"
        ENV
            "TF_DEBUG=MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS"
    )

endif()

# Test that USD render settings stored in a MayaUsdProxyShape node produce no
# path mapper warnings errors.  Launches Render and checks for success,
# no image comparison.
set(_usdProxyShapeRenderSettings_dir
    "${TEST_OUTPUT_ROOT}/usdProxyShapeRenderSettings/projects/default/images")
function(_add_usdProxyShapeRenderSettings_test test_name renderer output_subdir test_labels scene_rel_path)
    set(SCENE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/scenes/${scene_rel_path}")
    set(_output_dir "${TEST_OUTPUT_ROOT}/${output_subdir}/projects/default/images")

    if(WIN32)
        add_test(
            NAME "${test_name}"
            COMMAND PowerShell -Command
                "& \"${RENDER_EXECUTABLE}\" -renderer \"${renderer}\" -rd \"${_output_dir}\" \"${SCENE_PATH}\"; exit \$LASTEXITCODE"
        )
    else()
        add_test(
            NAME "${test_name}"
            COMMAND ${RENDER_EXECUTABLE} -renderer ${renderer} -rd "${_output_dir}" "${SCENE_PATH}"
        )
    endif()

    _mayaHydra_setup_test_common_path_vars()
    list(APPEND ALL_PATH_VARS MAYA_RENDER_DESC_PATH)
    _mayaHydra_setup_test_common_defaults("${test_name}")
    _mayaHydra_setup_test_plugins()
    list(APPEND MAYAHYDRA_VARNAME_MAYA_RENDER_DESC_PATH
         "${CMAKE_INSTALL_PREFIX}/renderDesc")
    list(APPEND MAYAHYDRA_VARNAME_PYTHONPATH "${MAYA_HYDRA_DIR}/scripts")
    _mayaHydra_setup_test_USD_paths()
    _mayaHydra_setup_test_finalize_env("${test_name}")

    if("${renderer}" STREQUAL "HdPrmanLoaderRendererPlugin")
        _mayaHydra_append_prman_production_render_env("${test_name}")
    endif()

    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
        "MAYA_IGNORE_DIALOGS=1")
    # Set TF_VERIFY to generate a TF_FATAL_ERROR.
    # PathMapperRegistry::UfePathToPrimSelections() use TF_VERIFY to check
    # for path mapper failures, so this will cause the test to fail.
    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
        "TF_FATAL_VERIFY=1")
    set_property(TEST "${test_name}" APPEND PROPERTY LABELS cmdLineRender)
    apply_labels_to_test("${test_labels}" ${test_name})

endfunction()
_add_usdProxyShapeRenderSettings_test(
    usdProxyShapeRenderSettings
    HdArnoldRendererPlugin
    usdProxyShapeRenderSettings
    ""
    "renderSettings/arnold/usdProxyShapeRenderSettings.ma")
if(IS_WINDOWS)
    _add_usdProxyShapeRenderSettings_test(
        usdProxyShapeRenderSettingsPRMan
        HdPrmanLoaderRendererPlugin
        usdProxyShapeRenderSettingsPRMan
        "prman"
        "renderSettings/prman/usdProxyShapeRenderSettings.ma")
endif()

# Test relative path resolution for Maya scene files.
# Scenes may be transferred across different environments (e.g., render farms),
# so the `-proj` argument is used to define the project root for resolving asset paths.
# This ensures that relative paths are correctly remapped at load time.
#
# Example of an absolute path embedded in a .ma file:
#   setAttr ".ftn" -type "string" "D:/nonExistentPath//UVChecker.png";
# The test verifies that such paths can be resolved relative to the specified project.
#
# Scene and baseline image live under relativePathTest/<renderer>/.
# Stage UVChecker.png within each test’s temporary Maya project so it can be
# resolved via the workspace root when using `-proj`.
#
# Expected test directory layout:
# .../test/Temporary/<sceneStem>/projects/default   ← what -proj points at
# ├── images/<sceneStem>.png
# └── sourceImages/UVChecker.png
function(_mayaHydra_add_relpath_variant folder)
    _mayaHydra_renderer_for_folder(${folder} renderer suffix)
    _mayaHydra_dep_label_for_folder(${folder} dep_label)
    set(scene_stem "mayaSphereMayaMaterial${suffix}RelativePath")

    set(proj_dir "${TEST_OUTPUT_ROOT}/${scene_stem}/projects/default")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/scenes/texturing/storm/UVChecker.png"
        "${proj_dir}/sourceImages/UVChecker.png"
        COPYONLY
    )

    set(render_dir ${TEST_OUTPUT_ROOT}/${scene_stem}/projects/default/images)
    set(scene relativePathTest/${folder}/${scene_stem}.ma)
    cmake_path(GET scene STEM sceneFileNameNoExt)
    mayaHydra_add_cmd_line_render_test(${scene}${dep_label}
        RENDERER ${renderer}
        # Must specify:
        # - The resolution, otherwise the USD default render settings 2048x1080
        #   will be used
        # - The render directory and image name, otherwise the USD default render
        #   settings ./default.png will be used, which places the rendered
        #   output in the source tree in the Maya scene file directory, with
        #   image name "default.png".
        RENDERER_ARGS "-x 160 -y 120 -proj \"${proj_dir}\" -rd \"${render_dir}\" -im ${sceneFileNameNoExt}"
    )
endfunction()

_mayaHydra_add_relpath_variant(arnold)
if(IS_WINDOWS)
    _mayaHydra_add_relpath_variant(prman)
endif()

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
# cmdLineRender is only registered for Maya > 2026 (see test/lib/CMakeLists.txt)
# because Hydra production rendering is not supported on Maya 2026.
set(MAYABATCH_TEST_SCRIPT_FILES
    testRenderingColorSpaceResolving.py|depOnPlugins:mtoa
    testActiveRenderDescriptionSceneGlobals.py|depOnPlugins:mtoa
)
# registeredMayaRenderers fails on Linux and macOS.
if(IS_WINDOWS)
    list(APPEND MAYABATCH_TEST_SCRIPT_FILES
        registeredMayaRenderers.py
    )
endif()

# Important: All tests in MAYABATCH_TEST_SCRIPT_FILES_TEST_MODE MUST release
# the batch renderer by calling cmds.mayaHydraTesting(releaseBatchRenderer=True).
# The batchRenderer object destruction is deferred so tests can inspect the hydra scene index tree.
# See the example in testActiveRenderDescriptionSceneGlobals.py
set(MAYABATCH_TEST_SCRIPT_FILES_TEST_MODE
    testRenderingColorSpaceResolving.py
    testActiveRenderDescriptionSceneGlobals.py
)

foreach(script ${MAYABATCH_TEST_SCRIPT_FILES})    
    get_testfile_and_labels(all_labels test_filename ${script})
    list(APPEND all_labels "cmdLineRender")
    mayaUsd_get_unittest_target(target ${test_filename})

    set(test_env "DISPLAY=:0")
    if(test_filename IN_LIST MAYABATCH_TEST_SCRIPT_FILES_TEST_MODE)
        list(APPEND test_env "MAYA_HYDRA_BATCH_RENDER_TEST_MODE=1")
    endif()

    mayaUsd_add_test(${target}
        PYTHON_SCRIPT ${test_filename}
        ENV ${test_env}
    )

    # Add a ctest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

# Hydra scene index dump tests.  Render with the -hsd flag to produce a text
# dump of the Hydra scene index, then compare the dump against an expected
# baseline using compareTextFiles.py.  Using a Python script rather than fc (or
# fc /W) on Windows / diff on UNIX to normalize line ending differences, for
# robustness.
function(_add_scene_index_dump_test test_name scene_file renderer_args dump_file expected_file)
    set(SCENE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/scenes/${scene_file}")
    set(COMPARE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/compareTextFiles.py")

    if(WIN32)
        set(CMD PowerShell)
        set(RM_ARGS "Remove-Item \"${dump_file}\" -Force -ErrorAction SilentlyContinue")
        set(RENDER_ARGS "& \"${RENDER_EXECUTABLE}\" -renderer \"HdArnoldRendererPlugin\" ${renderer_args} \"${SCENE_PATH}\"")
        set(COMPARE_ARGS "& \"${Python3_EXECUTABLE}\" \"${COMPARE_SCRIPT}\" \"${dump_file}\" \"${expected_file}\"")
        set(CMD_ARGS -Command "${RM_ARGS} \; ${RENDER_ARGS} \; if (\$LASTEXITCODE -eq 0) { ${COMPARE_ARGS} } \; exit \$LASTEXITCODE")
    else()
        set(CMD /bin/sh)
        set(RENDER_ARGS "\"${RENDER_EXECUTABLE}\" -renderer HdArnoldRendererPlugin ${renderer_args} \"${SCENE_PATH}\"")
        set(COMPARE_ARGS "\"${Python3_EXECUTABLE}\" \"${COMPARE_SCRIPT}\" \"${dump_file}\" \"${expected_file}\"")
        set(CMD_ARGS -c "rm -f \"${dump_file}\"; ${RENDER_ARGS} && ${COMPARE_ARGS}")
    endif()

    add_test(
        NAME "${test_name}"
        COMMAND ${CMD} ${CMD_ARGS}
    )

    _mayaHydra_setup_test_common_path_vars()
    list(APPEND ALL_PATH_VARS MAYA_RENDER_DESC_PATH)
    _mayaHydra_setup_test_common_defaults("${test_name}")
    _mayaHydra_setup_test_plugins()
    list(APPEND MAYAHYDRA_VARNAME_MAYA_RENDER_DESC_PATH
         "${CMAKE_INSTALL_PREFIX}/renderDesc")
    list(APPEND MAYAHYDRA_VARNAME_PYTHONPATH "${MAYA_HYDRA_DIR}/scripts")
    _mayaHydra_setup_test_USD_paths()
    _mayaHydra_setup_test_finalize_env("${test_name}")

    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
        "MAYA_IGNORE_DIALOGS=1")
    set_property(TEST "${test_name}" APPEND PROPERTY LABELS cmdLineRender)
endfunction()

# Two following tests fail on Windows 2027 and later builds on build machines,
# pass locally.  Entered as HYDRA-2408.
if(IS_LINUX)
    set(_terminal_dump_file
        "${TEST_OUTPUT_ROOT}/terminalSceneIndexDump/sceneIndexDump.txt")
    set(render_dir ${TEST_OUTPUT_ROOT}/terminalSceneIndexDump/projects/default/images)
    if(MAYA_APP_VERSION VERSION_GREATER 2027)
        set(_terminal_expected
            "${CMAKE_CURRENT_SOURCE_DIR}/expected/terminalSceneIndexDump_next.txt")
    else()
        set(_terminal_expected
            "${CMAKE_CURRENT_SOURCE_DIR}/expected/terminalSceneIndexDump_2027.txt")
    endif()
    _add_scene_index_dump_test(
        terminalSceneIndexDump
        texturing/arnold/mayaSphereMayaMaterialArnold.ma
        "-x 160 -y 120 -hsd \"${_terminal_dump_file}\" -rd \"${render_dir}\""
        "${_terminal_dump_file}"
        "${_terminal_expected}"
    )

    set(_mayaHydra_dump_file
        "${TEST_OUTPUT_ROOT}/mayaHydraSceneIndexDump/sceneIndexDump.txt")
    set(render_dir ${TEST_OUTPUT_ROOT}/mayaHydraSceneIndexDump/projects/default/images)
    _add_scene_index_dump_test(
        mayaHydraSceneIndexDump
        texturing/arnold/mayaSphereMayaMaterialArnold.ma
        "-x 160 -y 120 -hsd \"${_mayaHydra_dump_file}\" -hsi MayaHydraSceneIndex -rd \"${render_dir}\""
        "${_mayaHydra_dump_file}"
        "${CMAKE_CURRENT_SOURCE_DIR}/expected/mayaHydraSceneIndexDump.txt"
    )
endif()

# C++ unit tests
add_subdirectory(cpp)