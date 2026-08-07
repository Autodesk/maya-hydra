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

# CMake list() ignores empty elements by default (CMP0007 OLD). Use "." as a
# placeholder for an empty test dependency label and normalize when reading.
function(_mayaHydra_normalize_cmd_line_render_dep_label dep_label_var)
    if(${dep_label_var} STREQUAL ".")
        set(${dep_label_var} "" PARENT_SCOPE)
    endif()
endfunction()

# Register one <folder, renderer, suffix, dep_label, res_arg> row describing a
# per-renderer texturing test variant. Using a single macro call per row (as
# opposed to one list(APPEND ...) per column) keeps the parallel
# _tex_* lists below from drifting out of sync with each other.
macro(_mayaHydra_register_texturing_variant folder renderer suffix dep_label res_arg)
    list(APPEND _tex_folders ${folder})
    list(APPEND _tex_renderers ${renderer})
    list(APPEND _tex_suffixes ${suffix})
    list(APPEND _tex_dep_labels ${dep_label})
    list(APPEND _tex_res_args ${res_arg})
endmacro()

# Same as above, for the render settings production test variants
# (_rs_* lists), which additionally carry a "new image name" column.
macro(_mayaHydra_register_render_settings_variant folder renderer suffix dep_label res_arg new_image_name)
    list(APPEND _rs_folders ${folder})
    list(APPEND _rs_renderers ${renderer})
    list(APPEND _rs_suffixes ${suffix})
    list(APPEND _rs_dep_labels ${dep_label})
    list(APPEND _rs_res_args ${res_arg})
    list(APPEND _rs_new_image_names ${new_image_name})
endmacro()

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

# Texturing production tests. Scene + baseline live under texturing/<renderer>/.
if(IS_WINDOWS)
    _mayaHydra_register_texturing_variant(storm HdStormRendererPlugin Storm "." "-x 960 -y 540")
endif()
if(NOT IS_MACOSX)
    _mayaHydra_register_texturing_variant(arnold HdArnoldRendererPlugin Arnold "." "-x 160 -y 120")
endif()
if(IS_WINDOWS)
    _mayaHydra_register_texturing_variant(prman HdPrmanLoaderRendererPlugin PRMan "|depOnPlugins:prman" "-x 160 -y 120")
endif()

set(_tex_scene_stems
    mayaSphereMayaMaterial
    usdSphereMaterialX
    usdSphereUsdMaterial
)

list(LENGTH _tex_folders _tex_count)
if(_tex_count GREATER 0)
    math(EXPR _tex_last "${_tex_count} - 1")
    foreach(i RANGE ${_tex_last})
        list(GET _tex_folders ${i} _tex_folder)
        list(GET _tex_renderers ${i} _tex_renderer)
        list(GET _tex_suffixes ${i} _tex_suffix)
        list(GET _tex_dep_labels ${i} _tex_dep_label)
        _mayaHydra_normalize_cmd_line_render_dep_label(_tex_dep_label)
        list(GET _tex_res_args ${i} _tex_res_arg)

        foreach(_tex_stem ${_tex_scene_stems})
            set(_tex_scene_name "${_tex_stem}${_tex_suffix}")
            set(scene "texturing/${_tex_folder}/${_tex_scene_name}.ma")
            cmake_path(GET scene STEM sceneFileNameNoExt)
            if(_tex_folder STREQUAL "prman")
                set(render_dir ${CMAKE_BINARY_DIR}/test/Temporary/${sceneFileNameNoExt}/projects/default/images)
                mayaHydra_add_cmd_line_render_test(${scene}${_tex_dep_label}
                    COPY_SCENE
                    RENDERER ${_tex_renderer}
                    RENDERED_IMAGE_SUBDIR projects/default/images
                    RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
                    RENDERER_ARGS "${_tex_res_arg} -rd \"${render_dir}\" -im ${sceneFileNameNoExt}"
                )
            else()
                mayaHydra_add_cmd_line_render_test(${scene}${_tex_dep_label}
                    COPY_SCENE
                    RENDERER ${_tex_renderer}
                    RENDERED_IMAGE_SUBDIR projects/default/images
                    RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
                    RENDERER_ARGS "${_tex_res_arg} -rd ../images -im ${sceneFileNameNoExt}"
                )
            endif()
        endforeach()

        if(MAYA_APP_VERSION VERSION_GREATER 2026)
            set(_tex_scene_name "mayaSphereMaterialX${_tex_suffix}")
            set(scene "texturing/${_tex_folder}/${_tex_scene_name}.ma")
            cmake_path(GET scene STEM sceneFileNameNoExt)
            if(_tex_folder STREQUAL "prman")
                set(render_dir ${CMAKE_BINARY_DIR}/test/Temporary/${sceneFileNameNoExt}/projects/default/images)
                mayaHydra_add_cmd_line_render_test(${scene}${_tex_dep_label}
                    COPY_SCENE
                    RENDERER ${_tex_renderer}
                    RENDERED_IMAGE_SUBDIR projects/default/images
                    RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
                    RENDERER_ARGS "${_tex_res_arg} -rd \"${render_dir}\" -im ${sceneFileNameNoExt}"
                )
            else()
                mayaHydra_add_cmd_line_render_test(${scene}${_tex_dep_label}
                    COPY_SCENE
                    RENDERER ${_tex_renderer}
                    RENDERED_IMAGE_SUBDIR projects/default/images
                    RENDERED_IMAGE_NAME ${sceneFileNameNoExt}
                    RENDERER_ARGS "${_tex_res_arg} -rd ../images -im ${sceneFileNameNoExt}"
                )
            endif()
        endif()
    endforeach()
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
        set(render_dir ${CMAKE_BINARY_DIR}/test/Temporary/${sceneFileNameNoExt}_v2_rs/projects/default/images)
        mayaHydra_add_cmd_line_render_test(${scene}
            COPY_SCENE
            RENDERER HdArnoldRendererPlugin
            RENDERED_IMAGE_SUBDIR projects/default/images
            RENDERED_IMAGE_NAME ${sceneFileNameNoExt}_v2_rs
            RENDERER_ARGS "-x 160 -y 120 -rd \"${render_dir}\" -im ${sceneFileNameNoExt}_v2_rs"
            TEST_NAME_SUFFIX v2_rs
            ENV
                "MAYA_HYDRA_HD_ARNOLD_HYDRA_V2_RENDER_SETTINGS=1"
                "USDIMAGINGGL_ENGINE_ENABLE_SCENE_INDEX=1"
                "TF_DEBUG=MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS"
        )
    endforeach()
    if(MAYA_APP_VERSION VERSION_GREATER 2026)
        set(scene texturing/arnold/mayaSphereMaterialXArnold.ma)
        cmake_path(GET scene STEM sceneFileNameNoExt)
        set(render_dir ${CMAKE_BINARY_DIR}/test/Temporary/${sceneFileNameNoExt}_v2_rs/projects/default/images)
        mayaHydra_add_cmd_line_render_test(${scene}
            COPY_SCENE
            RENDERER HdArnoldRendererPlugin
            RENDERED_IMAGE_SUBDIR projects/default/images
            RENDERED_IMAGE_NAME ${sceneFileNameNoExt}_v2_rs
            RENDERER_ARGS "-x 160 -y 120 -rd \"${render_dir}\" -im ${sceneFileNameNoExt}_v2_rs"
            TEST_NAME_SUFFIX v2_rs
            ENV
                "MAYA_HYDRA_HD_ARNOLD_HYDRA_V2_RENDER_SETTINGS=1"
                "USDIMAGINGGL_ENGINE_ENABLE_SCENE_INDEX=1"
                "TF_DEBUG=MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS"
        )
    endif()
endif()

# Render settings production tests. Scene + baseline live under renderSettings/<renderer>/.
_mayaHydra_register_render_settings_variant(storm HdStormRendererPlugin Storm "." "-x 720 -y 480" aNewStormImageName)
_mayaHydra_register_render_settings_variant(arnold HdArnoldRendererPlugin Arnold "." "-x 720 -y 128" aNewArnoldImageName)
if(IS_WINDOWS)
    _mayaHydra_register_render_settings_variant(prman HdPrmanLoaderRendererPlugin PRMan "|depOnPlugins:prman" "-x 720 -y 128" aNewPRManImageName)
endif()

list(LENGTH _rs_folders _rs_count)
math(EXPR _rs_last "${_rs_count} - 1")
foreach(i RANGE ${_rs_last})
    list(GET _rs_folders ${i} _rs_folder)
    list(GET _rs_renderers ${i} _rs_renderer)
    list(GET _rs_suffixes ${i} _rs_suffix)
    list(GET _rs_dep_labels ${i} _rs_dep_label)
    _mayaHydra_normalize_cmd_line_render_dep_label(_rs_dep_label)
    list(GET _rs_res_args ${i} _rs_res_arg)
    list(GET _rs_new_image_names ${i} _rs_new_image_name)

    set(_rs_animcube_scene "renderSettings/${_rs_folder}/AnimCubeRenderSettings.ma")
    set(_rs_singlecam_scene "renderSettings/${_rs_folder}/AnimCubeRenderSettingsSingleCam.ma")

    set(_rs_scene "renderSettings/${_rs_folder}/${_rs_folder}Resolution.ma")
    if(_rs_folder STREQUAL "prman")
        set(render_dir ${TEST_OUTPUT_ROOT}/${_rs_folder}Resolution/projects/default/images)
        mayaHydra_add_cmd_line_render_test(${_rs_scene}${_rs_dep_label}
            COPY_SCENE
            RENDERER ${_rs_renderer}
            RENDERER_ARGS "${_rs_res_arg} -rd \"${render_dir}\" -im ${_rs_folder}Resolution"
        )
    else()
        mayaHydra_add_cmd_line_render_test(${_rs_scene}${_rs_dep_label}
            COPY_SCENE
            RENDERER ${_rs_renderer}
            RENDERER_ARGS "${_rs_res_arg} -rd ../images -im ${_rs_folder}Resolution"
        )
    endif()

    set(_rs_scene "renderSettings/${_rs_folder}/${_rs_folder}RenderDir.ma")
    if(_rs_folder STREQUAL "prman")
        set(render_dir ${TEST_OUTPUT_ROOT}/${_rs_folder}RenderDir/projects/default/images/aSubDirectory)
        file(MAKE_DIRECTORY "${render_dir}")
        mayaHydra_add_cmd_line_render_test(${_rs_scene}${_rs_dep_label}
            COPY_SCENE
            RENDERER ${_rs_renderer}
            RENDERED_IMAGE_SUBDIR projects/default/images/aSubDirectory
            RENDERER_ARGS "${_rs_res_arg} -rd \"${render_dir}\" -im ${_rs_folder}RenderDir"
        )
    else()
        mayaHydra_add_cmd_line_render_test(${_rs_scene}${_rs_dep_label}
            COPY_SCENE
            RENDERER ${_rs_renderer}
            RENDERED_IMAGE_SUBDIR projects/default/images/aSubDirectory
            RENDERER_ARGS "${_rs_res_arg} -rd ../images/aSubDirectory"
        )
    endif()

    set(_rs_scene "renderSettings/${_rs_folder}/${_rs_folder}ImageName.ma")
    if(_rs_folder STREQUAL "prman")
        set(render_dir ${TEST_OUTPUT_ROOT}/${_rs_folder}ImageName/projects/default/images)
        mayaHydra_add_cmd_line_render_test(${_rs_scene}${_rs_dep_label}
            COPY_SCENE
            RENDERER ${_rs_renderer}
            RENDERED_IMAGE_NAME ${_rs_new_image_name}
            RENDERER_ARGS "${_rs_res_arg} -rd \"${render_dir}\" -im ${_rs_new_image_name}"
        )
    else()
        mayaHydra_add_cmd_line_render_test(${_rs_scene}${_rs_dep_label}
            COPY_SCENE
            RENDERER ${_rs_renderer}
            RENDERED_IMAGE_NAME ${_rs_new_image_name}
            RENDERER_ARGS "${_rs_res_arg} -rd ../images -im ${_rs_new_image_name}"
        )
    endif()

    set(_rs_scene "renderSettings/${_rs_folder}/${_rs_folder}OutputFormat.ma")
    if(_rs_folder STREQUAL "prman")
        set(render_dir ${TEST_OUTPUT_ROOT}/${_rs_folder}OutputFormat/projects/default/images)
        mayaHydra_add_cmd_line_render_test(${_rs_scene}${_rs_dep_label}
            COPY_SCENE
            RENDERER ${_rs_renderer}
            IMAGE_EXTENSION exr
            RENDERER_ARGS "${_rs_res_arg} -rd \"${render_dir}\" -im ${_rs_folder}OutputFormat -of exr"
        )
    else()
        mayaHydra_add_cmd_line_render_test(${_rs_scene}${_rs_dep_label}
            COPY_SCENE
            RENDERER ${_rs_renderer}
            IMAGE_EXTENSION jpg
            RENDERER_ARGS "${_rs_res_arg} -rd ../images -im ${_rs_folder}OutputFormat -of jpg"
        )
    endif()

    if(_rs_folder STREQUAL "storm")
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
        set(render_dir ${TEST_OUTPUT_ROOT}/${_rs_folder}CropRegion/projects/default/images)
        set(_rs_scene "renderSettings/${_rs_folder}/${_rs_folder}CropRegion.ma")
        cmake_path(GET _rs_scene STEM sceneFileNameNoExt)
        mayaHydra_add_cmd_line_render_test(${_rs_scene}${_rs_dep_label}
            RENDERER ${_rs_renderer}
            RENDERER_ARGS "-x 160 -y 120 -reg 32 95 30 89 -rd \"${render_dir}\" -im ${sceneFileNameNoExt}"
            FAIL 0.5
        )
    endif()

    # Failure with HdStorm on macOS entered as HYDRA-2139. ("prman" never
    # appears in _rs_folders on macOS, so this reduces to NOT IS_MACOSX for
    # storm/arnold and is always true for prman.)
    if(NOT IS_MACOSX)
        set(_rs_multi_fail_args "")
        if(NOT _rs_folder STREQUAL "storm")
            set(_rs_multi_fail_args FAIL 0.05 FAILPERCENT 1.0)
        endif()
        mayaHydra_add_cmd_line_render_multi_image_test(${_rs_animcube_scene}${_rs_dep_label}
            RENDERER ${_rs_renderer}
            EXPECTED_IMAGES_DIR renderSettings/${_rs_folder}/images
            IMAGE_EXTENSION png
            TEST_NAME_SUFFIX ${_rs_suffix}
            ${_rs_multi_fail_args}
        )
    endif()

    # Test frame range (5-9).  Failure with HdStorm on macOS entered as HYDRA-2139.
    if(_rs_folder STREQUAL "storm")
        if(NOT IS_MACOSX)
            mayaHydra_add_cmd_line_render_multi_image_test(${_rs_animcube_scene}${_rs_dep_label}
                RENDERER ${_rs_renderer}
                EXPECTED_IMAGES_DIR renderSettings/${_rs_folder}/images/frames5to9
                IMAGE_EXTENSION png
                TEST_NAME_SUFFIX ${_rs_suffix}Frames5to9
                RENDERER_ARGS "-s 5 -e 9"
            )
        endif()
    else()
        mayaHydra_add_cmd_line_render_multi_image_test(${_rs_animcube_scene}${_rs_dep_label}
            RENDERER ${_rs_renderer}
            EXPECTED_IMAGES_DIR renderSettings/${_rs_folder}/images/frames5to9
            IMAGE_EXTENSION png
            TEST_NAME_SUFFIX ${_rs_suffix}Frames5to9
            FAIL 0.05
            FAILPERCENT 1.0
            RENDERER_ARGS "-s 5 -e 9"
        )
    endif()

    set(_rs_singlecam_fail_args FAIL 0.05 FAILPERCENT 1.0)
    if(_rs_folder STREQUAL "storm")
        set(_rs_singlecam_fail_args "")
    endif()
    mayaHydra_add_cmd_line_render_multi_image_test(${_rs_singlecam_scene}${_rs_dep_label}
        RENDERER ${_rs_renderer}
        EXPECTED_IMAGES_DIR renderSettings/${_rs_folder}/images/frames5to9
        RENDERED_IMAGE_SUBDIR projects/default/images/aSubDirectory
        IMAGE_EXTENSION png
        TEST_NAME_SUFFIX ${_rs_suffix}RenderDir
        ${_rs_singlecam_fail_args}
        RENDERER_ARGS "-rd images/aSubDirectory"
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
set(render_dir ${CMAKE_BINARY_DIR}/test/Temporary/${sceneFileNameNoExt}_Arnold_v2_rs/projects/default/images)
cmake_path(GET scene STEM sceneFileNameNoExt)
mayaHydra_add_cmd_line_render_test(${scene}
    COPY_SCENE
    RENDERER HdArnoldRendererPlugin
    RENDERED_IMAGE_SUBDIR projects/default/images
    RENDERED_IMAGE_NAME ${sceneFileNameNoExt}_Arnold_v2_rs
    RENDERER_ARGS "-x 960 -y 540 -cam \"|stage1|stageShape1,/camera1\" -rd \"${render_dir}\" -im ${sceneFileNameNoExt}_Arnold_v2_rs"
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

    # Same as above (Hydra V2) with render-settings debug logging enabled.
    set(scene basic/usdCamera.ma)
    set(render_dir ${CMAKE_BINARY_DIR}/test/Temporary/${sceneFileNameNoExt}_PRMan_v2_rs/projects/default/images)
    cmake_path(GET scene STEM sceneFileNameNoExt)
    mayaHydra_add_cmd_line_render_test(${scene}|depOnPlugins:prman
        COPY_SCENE
        RENDERER HdPrmanLoaderRendererPlugin
        RENDERED_IMAGE_SUBDIR projects/default/images
        RENDERED_IMAGE_NAME ${sceneFileNameNoExt}_PRMan_v2_rs
        TEST_NAME_SUFFIX PRMan_v2_rs
        RENDERER_ARGS "-x 960 -y 540 -cam \"|stage1|stageShape1,/camera1\" -rd \"${render_dir}\" -im ${sceneFileNameNoExt}_PRMan_v2_rs"
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
set(_relpath_folders arnold)
set(_relpath_scene_stems mayaSphereMayaMaterialArnoldRelativePath)
set(_relpath_renderers HdArnoldRendererPlugin)
set(_relpath_dep_labels ".")
if(IS_WINDOWS)
    list(APPEND _relpath_folders prman)
    list(APPEND _relpath_scene_stems mayaSphereMayaMaterialPRManRelativePath)
    list(APPEND _relpath_renderers HdPrmanLoaderRendererPlugin)
    list(APPEND _relpath_dep_labels "|depOnPlugins:prman")
endif()

list(LENGTH _relpath_folders _relpath_count)
math(EXPR _relpath_last "${_relpath_count} - 1")
foreach(i RANGE ${_relpath_last})
    list(GET _relpath_folders ${i} _relpath_folder)
    list(GET _relpath_scene_stems ${i} _relpath_scene_stem)
    list(GET _relpath_renderers ${i} _relpath_renderer)
    list(GET _relpath_dep_labels ${i} _relpath_dep_label)
    _mayaHydra_normalize_cmd_line_render_dep_label(_relpath_dep_label)

    set(_relpath_proj_dir
        "${TEST_OUTPUT_ROOT}/${_relpath_scene_stem}/projects/default")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/scenes/texturing/storm/UVChecker.png"
        "${_relpath_proj_dir}/sourceImages/UVChecker.png"
        COPYONLY
    )

    set(render_dir ${TEST_OUTPUT_ROOT}/${_relpath_scene_stem}/projects/default/images)
    set(scene relativePathTest/${_relpath_folder}/${_relpath_scene_stem}.ma)
    cmake_path(GET scene STEM sceneFileNameNoExt)
    mayaHydra_add_cmd_line_render_test(${scene}${_relpath_dep_label}
        RENDERER ${_relpath_renderer}
        # Must specify:
        # - The resolution, otherwise the USD default render settings 2048x1080
        #   will be used
        # - The render directory and image name, otherwise the USD default render
        #   settings ./default.png will be used, which places the rendered
        #   output in the source tree in the Maya scene file directory, with
        #   image name "default.png".
        RENDERER_ARGS "-x 160 -y 120 -proj \"${_relpath_proj_dir}\" -rd \"${render_dir}\" -im ${sceneFileNameNoExt}"
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
