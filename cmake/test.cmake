set(MAYA_HYDRA_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Paths to append to PXR_PLUGINPATH_NAME for tests (e.g. HdArnold plugin).
# Sources (first wins): -DADDITIONAL_PXR_PLUGINPATH_NAME=... or $ENV{ADDITIONAL_PXR_PLUGINPATH_NAME}
# On Windows use forward slashes or escaped backslashes.
if(NOT DEFINED ADDITIONAL_PXR_PLUGINPATH_NAME)
    set(ADDITIONAL_PXR_PLUGINPATH_NAME "" CACHE STRING
        "Semicolon-separated paths to append to PXR_PLUGINPATH_NAME for tests (e.g. HdArnold)")
endif()
if(NOT ADDITIONAL_PXR_PLUGINPATH_NAME AND DEFINED ENV{ADDITIONAL_PXR_PLUGINPATH_NAME})
    set(ADDITIONAL_PXR_PLUGINPATH_NAME "$ENV{ADDITIONAL_PXR_PLUGINPATH_NAME}" CACHE STRING
        "Semicolon-separated paths to append to PXR_PLUGINPATH_NAME for tests (e.g. HdArnold)" FORCE)
endif()

if(MayaUsd_FOUND)
    if(IS_MACOSX OR IS_LINUX) 
        #When MayaUsd_FOUND is true, MAYAUSDAPI_LIBRARY exists as it is required. 
        #MAYAUSDAPI_LIBRARY is the full path name of the maya USD API shared library, so get only its directory into MAYAUSDAPI_LIBRARY_PATH
        get_filename_component(MAYAUSDAPI_LIBRARY_PATH "${MAYAUSDAPI_LIBRARY}" DIRECTORY)

        #So add MAYAUSDAPI_LIBRARY_PATH to the ADDITIONAL_LD_LIBRARY_PATH which is used to run the tests
        # Also, add paths to USD libraries, which render delegates link against.
        set(CURRENT_ADDITIONAL_LD_LIBRARY_PATH $ENV{ADDITIONAL_LD_LIBRARY_PATH})
        set(ADDITIONAL_LD_LIBRARY_PATH "${CURRENT_ADDITIONAL_LD_LIBRARY_PATH}:${MAYAUSDAPI_LIBRARY_PATH}:${PXR_USD_LOCATION}/lib64:${PXR_USD_LOCATION}/lib")

        # Export the new value to the environment
        set(ENV{ADDITIONAL_LD_LIBRARY_PATH} ${ADDITIONAL_LD_LIBRARY_PATH})
        message(STATUS "ADDITIONAL_LD_LIBRARY_PATH is now : ${ADDITIONAL_LD_LIBRARY_PATH}")
    endif()

    if (MAYAUSD_MOD_PATH)
        #Add MAYAUSD_MOD_PATH (the path where maya USD .mod file is) to the MAYA_MODULE_PATH
        # Get the current value of the environment variable
        set(CURRENT_MAYA_MODULE_PATH $ENV{MAYA_MODULE_PATH})
        # Append the new path to the current value
        if(IS_MACOSX OR IS_LINUX) 
            #Linux and OSX
            set(MAYA_MODULE_PATH "${CURRENT_MAYA_MODULE_PATH}:${MAYAUSD_MOD_PATH}")
        else()
            #Windows
            set(MAYA_MODULE_PATH "${CURRENT_MAYA_MODULE_PATH}\;${MAYAUSD_MOD_PATH}")
        endif()
        # Export the new value to the environment
        set(ENV{MAYA_MODULE_PATH} ${MAYA_MODULE_PATH})
        message(STATUS "MAYA_MODULE_PATH is now : ${MAYA_MODULE_PATH}")
    endif()
endif()

function(find_labels label_set label_list)
    string(REPLACE ":" ";" split_labels ${label_set})
    list(LENGTH split_labels len)
    if(len GREATER 0)
        list(GET split_labels 1 labels_value)
        # we expect comma separated labels
        string(REPLACE "," ";" all_labels ${labels_value})
        set(local_label_list "")
        foreach(label ${all_labels})
            list(APPEND local_label_list ${label})
        endforeach()
        set(${label_list} ${local_label_list} PARENT_SCOPE)
    endif()
endfunction()

#
# mayaHydra_add_cmd_line_render_multi_image_test( <scene_file_labeled>
#                           [RENDERER <renderer_name>]
#                           [EXPECTED_IMAGES_DIR <dir>]
#                           [IMAGE_EXTENSION <extension>]
#                           [FAIL <idiff fail value>]
#                           [FAILPERCENT <idiff failpercent value>]
#                           [WORKING_DIRECTORY <dir>]
#                           [TEST_NAME_SUFFIX <suffix>]
#                           [ENV <varname>=<varvalue> ...])
#
# Similar to mayaHydra_add_cmd_line_render_test but compares multiple output images.
# The expected images directory is compared against the rendered output directory.
#
function(mayaHydra_add_cmd_line_render_multi_image_test SCENE_FILE_LABELED)
    cmake_parse_arguments(ARG
        ""                                       # No boolean options.
        "RENDERER;SCENE_FILE;WORKING_DIRECTORY;IMAGE_EXTENSION;FAIL;FAILPERCENT;EXPECTED_IMAGES_DIR;TEST_NAME_SUFFIX"
        "ENV"
        ${ARGN}
    )

    if(ARG_WORKING_DIRECTORY)
        set(WORKING_DIR ${ARG_WORKING_DIRECTORY})
    else()
        set(WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    set(RENDERER "HdStormRendererPlugin")
    if(ARG_RENDERER)
        set(RENDERER "${ARG_RENDERER}")
    endif()

    if(ARG_SCENE_FILE_LABELED)
        set(SCENE_FILE_LABELED "${ARG_SCENE_FILE_LABELED}")
    endif()

    get_testfile_and_labels(ALL_LABELS SCENE_FILE ${SCENE_FILE_LABELED})
    mayaUsd_get_unittest_target(test_name ${SCENE_FILE})
    if(ARG_TEST_NAME_SUFFIX)
        set(test_name "${test_name}_${ARG_TEST_NAME_SUFFIX}")
    endif()

    set(IMAGE_EXTENSION "exr")
    if(ARG_IMAGE_EXTENSION)
        set(IMAGE_EXTENSION "${ARG_IMAGE_EXTENSION}")
    endif()

    set(FAIL "0.01")
    if(ARG_FAIL)
        set(FAIL "${ARG_FAIL}")
    endif()

    set(FAILPERCENT "1.0")
    if(ARG_FAILPERCENT)
        set(FAILPERCENT "${ARG_FAILPERCENT}")
    endif()

    set(SCENE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scenes)
    set(SCENE_PATH ${SCENE_DIR}/${SCENE_FILE})

    if(ARG_EXPECTED_IMAGES_DIR)
        if(IS_ABSOLUTE "${ARG_EXPECTED_IMAGES_DIR}")
            set(EXPECTED_IMAGES_DIR "${ARG_EXPECTED_IMAGES_DIR}")
        else()
            set(EXPECTED_IMAGES_DIR "${SCENE_DIR}/${ARG_EXPECTED_IMAGES_DIR}")
        endif()
    else()
        set(EXPECTED_IMAGES_DIR "${SCENE_DIR}")
    endif()

    if (IMAGE_DIFF_TOOL)
        set(IDIFF_CMD "${IMAGE_DIFF_TOOL}")
    else()
        message(FATAL_ERROR "idiff binary not discovered. Set IMAGE_DIFF_TOOL (e.g. via OIIO_idiff_BINARY).")
    endif()

    add_test(
        NAME "${test_name}"
        WORKING_DIRECTORY ${WORKING_DIR}
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/renderSettingsMultiImageTest.py
                ${RENDER_EXECUTABLE}
                ${RENDERER}
                ${SCENE_PATH}
                ${EXPECTED_IMAGES_DIR}
                ${IDIFF_CMD}
                ${FAIL}
                ${FAILPERCENT}
    )

    _mayaHydra_setup_test_common_path_vars()
    list(APPEND ALL_PATH_VARS MAYA_RENDER_DESC_PATH)

    _mayaHydra_setup_test_common_defaults("${test_name}")
    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
        "MAYA_DEFAULT_SURFACE_SHADER=standardSurface")

    _mayaHydra_setup_test_plugins()
    list(APPEND MAYAHYDRA_VARNAME_MAYA_RENDER_DESC_PATH
         "${CMAKE_INSTALL_PREFIX}/renderDesc")

    _mayaHydra_setup_test_USD_paths()
    _mayaHydra_setup_test_finalize_env("${test_name}")

    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
        "MAYA_IGNORE_DIALOGS=1")

    set_property(TEST "${test_name}" APPEND PROPERTY LABELS cmdLineRender)
    apply_labels_to_test("${ALL_LABELS}" ${test_name})
endfunction()

function(get_testfile_and_labels all_labels test_filename test_script)
    # fetch labels for each test file
    string(REPLACE "|" ";" tests_with_tags ${test_script})
    list(GET tests_with_tags 0 filename)
    # set the test file to input as no labels were passed
    set(${test_filename} ${filename} PARENT_SCOPE)
    list(LENGTH tests_with_tags length)
    math(EXPR one_less_length "${length} - 1")
    if(length GREATER 1)        
        set(collect_labels "")
        foreach(i RANGE 1 ${one_less_length})
            list(GET tests_with_tags ${i} item)
            find_labels(${item} label_list)
            list(APPEND collect_labels ${label_list})
        endforeach()
        set(${all_labels} ${collect_labels} PARENT_SCOPE)    
    else()
        set(${all_labels} "" PARENT_SCOPE)
    endif()
endfunction()

function(apply_labels_to_test test_labels test_file)
    set_property(TEST ${test_file} APPEND PROPERTY LABELS "default")
    list(LENGTH test_labels list_length)
    if(${list_length} GREATER 0)
    # if(NOT ${test_labels} STREQUAL "")
        foreach(label ${test_labels})
            set_property(TEST ${test_file} APPEND PROPERTY LABELS ${label})
            message(STATUS "Added test label \"${label}\" for ${test_file}")
        endforeach()
    endif()
endfunction()

function(mayaUsd_get_unittest_target unittest_target unittest_basename)
    get_filename_component(unittest_name ${unittest_basename} NAME_WE)
    set(${unittest_target} "${unittest_name}" PARENT_SCOPE)
endfunction()
# Set up common path environment variables for Maya Hydra tests
function(_mayaHydra_setup_test_common_path_vars)
    set(ALL_PATH_VARS
        PYTHONPATH
        MAYA_MODULE_PATH
        MAYA_PLUG_IN_PATH
        MAYA_SCRIPT_PATH
        XBMLANGPATH
        ${PXR_OVERRIDE_PLUGINPATH_NAME}
        PXR_MTLX_STDLIB_SEARCH_PATHS
        MATERIALX_SEARCH_PATH
    )

    if(IS_WINDOWS)
        # Put path at the front of the list of env vars.
        list(INSERT ALL_PATH_VARS 0 PATH)
    else()
        list(APPEND ALL_PATH_VARS
             LD_LIBRARY_PATH
             IDIFF_LD_LIBRARY_PATH
             LD_PRELOAD
        )
    endif()

    # Set initial empty values for all path vars
    # NOTE - we prefix varnames with "MAYAHYDRA_VARNAME_" just to make collision
    # with some existing var less likely
    foreach(pathvar ${ALL_PATH_VARS})
        set(MAYAHYDRA_VARNAME_${pathvar} "" PARENT_SCOPE)
    endforeach()

    if(IS_WINDOWS)
        list(APPEND MAYAHYDRA_VARNAME_PATH "${MAYA_LOCATION}/bin")
        set(MAYAHYDRA_VARNAME_PATH ${MAYAHYDRA_VARNAME_PATH} PARENT_SCOPE)
    else()
        # Set up environment for idiff execution
        set(MAYAHYDRA_VARNAME_LD_LIBRARY_PATH "${ADDITIONAL_LD_LIBRARY_PATH}" PARENT_SCOPE)
    
        # LD_LIBRARY_PATH needs to be set for the idiff executable because its 
        # RPATH is absolute rather than relative to ORIGIN, meaning the RPATH 
        # points to the absolute path on the machine where idiff was built.
        # This absence of relative paths for RPATH comes from OpenImageIO.
        # We introduce a second workaround to avoid Maya using usd's libpng, 
        # because both use incompatible versions of libpng. This is done by 
        # setting LD_LIBRARY_PATH to IDIFF_LD_LIBRARY_PATH only when we run 
        # idiff using Python's subprocess module.
        set(MAYAHYDRA_VARNAME_IDIFF_LD_LIBRARY_PATH "${ADDITIONAL_LD_LIBRARY_PATH}:${PXR_USD_LOCATION}/lib64:${PXR_USD_LOCATION}/lib" PARENT_SCOPE)
    
        # Maya uses a very old version of GLEW, so we need support for
        # pre-loading a newer version from elsewhere.
        set(MAYAHYDRA_VARNAME_LD_PRELOAD "${ADDITIONAL_LD_PRELOAD}" PARENT_SCOPE)
    endif()

    # Return the list of path variables
    set(ALL_PATH_VARS ${ALL_PATH_VARS} PARENT_SCOPE)
endfunction()

# Set up environment for overall test and Maya defaults. 
function(_mayaHydra_setup_test_common_defaults test_name)
    set(ALL_TEST_VARS
        IMAGE_DIFF_TOOL
        OIIOTOOL
        MAYA_HAS_RENDER_ITEM_CULL_MODE_API
    )
    # Configure default PRMan allowed platforms for tests.
    list(APPEND ALL_TEST_VARS MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS)
    if(IS_WINDOWS)
        set(MAYAHYDRA_VARNAME_MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS "windows")
    else()
        set(MAYAHYDRA_VARNAME_MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS "")
    endif()

    set(MAYAHYDRA_VARNAME_IMAGE_DIFF_TOOL "${IMAGE_DIFF_TOOL}")
    set(MAYAHYDRA_VARNAME_OIIOTOOL "${OIIOTOOL}")

    set(MAYAHYDRA_VARNAME_MAYA_HAS_RENDER_ITEM_CULL_MODE_API "${MAYA_HAS_RENDER_ITEM_CULL_MODE_API}")

    if(CODE_COVERAGE)
        list(APPEND ALL_TEST_VARS MAYAHYDRA_CODE_COVERAGE)
        set(MAYAHYDRA_VARNAME_MAYAHYDRA_CODE_COVERAGE "1")
    endif()

    if(IS_MACOSX)
        # Dump failing HdSt shader sources to help diagnose Metal compilation issues.
        set(_tf_debug "$ENV{TF_DEBUG}")
        if(_tf_debug)
            set(_tf_debug "${_tf_debug},HDST_DUMP_FAILING_SHADER_SOURCEFILE")
        else()
            set(_tf_debug "HDST_DUMP_FAILING_SHADER_SOURCEFILE")
        endif()
        list(APPEND ALL_TEST_VARS TF_DEBUG)
        set(MAYAHYDRA_VARNAME_TF_DEBUG "${_tf_debug}")
    endif()

    foreach(testvar ${ALL_TEST_VARS})
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "${testvar}=${MAYAHYDRA_VARNAME_${testvar}}")
    endforeach()
endfunction()

# Emulate what the module files for mayaHydra, mayaUsdPlugin, and other plugins
# would do.
function(_mayaHydra_setup_test_plugins)
    # mayaHydra
    list(APPEND MAYAHYDRA_VARNAME_PATH
         "${CMAKE_INSTALL_PREFIX}/lib")
    list(APPEND MAYAHYDRA_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME}
         "${CMAKE_INSTALL_PREFIX}/lib/usd")
    list(APPEND MAYAHYDRA_VARNAME_MAYA_PLUG_IN_PATH
         "${CMAKE_INSTALL_PREFIX}/lib/maya")
    list(APPEND MAYAHYDRA_VARNAME_MAYA_SCRIPT_PATH
         "${CMAKE_INSTALL_PREFIX}/scripts")

    # mayaUsdPlugin
    if(DEFINED MAYAUSD_LOCATION)
        list(APPEND MAYAHYDRA_VARNAME_PATH
             "${MAYAUSD_LOCATION}/lib")
        list(APPEND MAYAHYDRA_VARNAME_PYTHONPATH
             "${MAYAUSD_LOCATION}/lib/scripts")
        list(APPEND MAYAHYDRA_VARNAME_MAYA_SCRIPT_PATH
             "${MAYAUSD_LOCATION}/lib/scripts")
        if (IS_LINUX)
            # On Linux the paths in XBMLANGPATH need a /%B at the end.
            list(APPEND MAYAHYDRA_VARNAME_XBMLANGPATH
                 "${MAYAUSD_LOCATION}/lib/icons/%B")
        else()
            list(APPEND MAYAHYDRA_VARNAME_XBMLANGPATH
                 "${MAYAUSD_LOCATION}/lib/icons")
        endif()
        list(APPEND MAYAHYDRA_VARNAME_PYTHONPATH 
             "${MAYAUSD_LOCATION}/lib/python")
        # USD plugin paths:
        # - Windows: prepend OpenUSD (for PRMan delegate discovery), then MayaUSD lib/usd.
        # - macOS/Linux: use MayaUSD lib/usd only.
        # Do NOT add PXR_USD_LOCATION/lib/usd on macOS: those plugins link against a
        # different USD build than Maya's bundled USD, causing duplicate TF_DEBUG_ENVIRONMENT_SYMBOL
        # registration (e.g. HGIMETAL_DEBUG_ERROR_STACKTRACE) that fatally crashes every test.
        if(IS_WINDOWS AND DEFINED PXR_USD_LOCATION AND EXISTS "${PXR_USD_LOCATION}/lib/usd")
            list(APPEND MAYAHYDRA_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME}
                 "${PXR_USD_LOCATION}/lib/usd")
            message(STATUS "Using OpenUSD plugin path from PXR_USD_LOCATION: ${PXR_USD_LOCATION}/lib/usd")
        endif()
        list(APPEND MAYAHYDRA_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME}
             "${MAYAUSD_LOCATION}/lib/usd")
        list(APPEND MAYAHYDRA_VARNAME_MAYA_PLUG_IN_PATH
             "${MAYAUSD_LOCATION}/plugin/adsk/plugin")
        list(APPEND MAYAHYDRA_VARNAME_PYTHONPATH
             "${MAYAUSD_LOCATION}/plugin/adsk/scripts")
        list(APPEND MAYAHYDRA_VARNAME_MAYA_SCRIPT_PATH
             "${MAYAUSD_LOCATION}/plugin/adsk/scripts")
        list(APPEND MAYAHYDRA_VARNAME_PXR_MTLX_STDLIB_SEARCH_PATHS
             "${PXR_USD_LOCATION}/libraries")
        list(APPEND MAYAHYDRA_VARNAME_PXR_MTLX_STDLIB_SEARCH_PATHS
             "${MAYAUSD_LOCATION}/libraries")
    endif()

    # Additional plugin paths (e.g. HdArnold) for tests that need them.
    # On OSX/Linux, exclude PRMan paths to avoid TfType redefinition errors.
    if(ADDITIONAL_PXR_PLUGINPATH_NAME)
        foreach(extra_path ${ADDITIONAL_PXR_PLUGINPATH_NAME})
            if(IS_WINDOWS)
                list(APPEND MAYAHYDRA_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME} "${extra_path}")
            else()
                string(TOLOWER "${extra_path}" _path_lower)
                if(NOT _path_lower MATCHES "prman|hdprman|renderman|rman")
                    list(APPEND MAYAHYDRA_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME} "${extra_path}")
                endif()
            endif()
        endforeach()
        message(STATUS "ADDITIONAL_PXR_PLUGINPATH_NAME for tests: ${ADDITIONAL_PXR_PLUGINPATH_NAME}")
    endif()
    
    # mtoa
    if(DEFINED MTOA_LOCATION)
        # It seems like we need to use MAYA_MODULE_PATH for MtoA to work properly.
        # Even if we emulate the .mod file by manually setting the same env vars
        # to the same values, MtoA itself will appear to load successfully when 
        # calling loadPlugin, but some of its extensions will fail to initialize,
        # leading to incorrect behavior and test failures. In those cases, it seems
        # like having a locally installed MtoA fixed it, but we can't rely on that.
        list(APPEND MAYAHYDRA_VARNAME_MAYA_MODULE_PATH
             "${MTOA_LOCATION}")
        # Hydra Arnold render delegate plugin path. Try both layouts: newer Arnold
        # uses usd/bundle/<version>, older uses usd/hydra/<version>. Version is USD
        # without "." (e.g. 2511 for USD 0.25.11). Add only the path that contains
        # plugInfo.json.
        if(IS_WINDOWS AND DEFINED USD_VERSION)
            string(REGEX REPLACE "^0\\.([0-9]+)\\.([0-9]+)$" "\\1\\2" MTOA_USD_VERSION_HYDRA "${USD_VERSION}")
            set(MTOA_HYDRA_BUNDLE "${MTOA_LOCATION}/usd/bundle/${MTOA_USD_VERSION_HYDRA}")
            set(MTOA_HYDRA_LEGACY "${MTOA_LOCATION}/usd/hydra/${MTOA_USD_VERSION_HYDRA}")
            if(EXISTS "${MTOA_HYDRA_BUNDLE}/plugInfo.json")
                list(APPEND MAYAHYDRA_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME} "${MTOA_HYDRA_BUNDLE}")
            elseif(EXISTS "${MTOA_HYDRA_LEGACY}/plugInfo.json")
                list(APPEND MAYAHYDRA_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME} "${MTOA_HYDRA_LEGACY}")
            endif()
        endif()
    endif()

    # prman: delegate path (extracted package, not merged into USD) and runtime
    # Platform selection driven by .yaml; if vars are set, configure test env.
    # Sources (first wins): -DVar=... or $ENV{Var} (ENV fallback for CI/standard RenderMan setup)
    if((NOT DEFINED PRMAN_DELEGATE_PLUGIN_PATH OR "${PRMAN_DELEGATE_PLUGIN_PATH}" STREQUAL "") AND DEFINED ENV{PRMAN_DELEGATE_PLUGIN_PATH} AND NOT "$ENV{PRMAN_DELEGATE_PLUGIN_PATH}" STREQUAL "")
        set(PRMAN_DELEGATE_PLUGIN_PATH "$ENV{PRMAN_DELEGATE_PLUGIN_PATH}" CACHE PATH "Path containing HdPrman plugInfo.json" FORCE)
    endif()
    if((NOT DEFINED RMANTREE OR "${RMANTREE}" STREQUAL "") AND DEFINED ENV{RMANTREE} AND NOT "$ENV{RMANTREE}" STREQUAL "")
        set(RMANTREE "$ENV{RMANTREE}" CACHE PATH "RenderMan installation root" FORCE)
    endif()
    if((NOT DEFINED RENDERMAN_LOCATION OR "${RENDERMAN_LOCATION}" STREQUAL "") AND DEFINED ENV{RENDERMAN_LOCATION} AND NOT "$ENV{RENDERMAN_LOCATION}" STREQUAL "")
        set(RENDERMAN_LOCATION "$ENV{RENDERMAN_LOCATION}" CACHE PATH "RenderMan location (optional)" FORCE)
    endif()
    if((NOT DEFINED PIXAR_LICENSE_FILE OR "${PIXAR_LICENSE_FILE}" STREQUAL "") AND DEFINED ENV{PIXAR_LICENSE_FILE} AND NOT "$ENV{PIXAR_LICENSE_FILE}" STREQUAL "")
        set(PIXAR_LICENSE_FILE "$ENV{PIXAR_LICENSE_FILE}" CACHE STRING "Pixar license server (port@hostname)" FORCE)
    endif()
    # Only enable PRMan on Windows: tests and plugins are supported there.
    # On OSX/Linux, loading PRMan artifacts causes TfType redefinition errors
    # (UsdSkelImaging* already defined) due to conflicting USD plugin loads.
    if(IS_WINDOWS)
        if(DEFINED PRMAN_DELEGATE_PLUGIN_PATH AND NOT "${PRMAN_DELEGATE_PLUGIN_PATH}" STREQUAL "")
            list(APPEND MAYAHYDRA_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME}
                 "${PRMAN_DELEGATE_PLUGIN_PATH}")
            list(APPEND ALL_TEST_VARS PRMAN_DELEGATE_PLUGIN_PATH)
            set(MAYAHYDRA_VARNAME_PRMAN_DELEGATE_PLUGIN_PATH "${PRMAN_DELEGATE_PLUGIN_PATH}")
        endif()
        if(DEFINED RMANTREE AND NOT "${RMANTREE}" STREQUAL "")
            list(APPEND ALL_TEST_VARS RMANTREE)
            set(MAYAHYDRA_VARNAME_RMANTREE "${RMANTREE}")
            list(APPEND MAYAHYDRA_VARNAME_PATH "${RMANTREE}/bin")
            list(APPEND MAYAHYDRA_VARNAME_PATH "${RMANTREE}/lib")
        endif()
        if(DEFINED RENDERMAN_LOCATION AND NOT "${RENDERMAN_LOCATION}" STREQUAL "")
            list(APPEND ALL_TEST_VARS RENDERMAN_LOCATION)
            set(MAYAHYDRA_VARNAME_RENDERMAN_LOCATION "${RENDERMAN_LOCATION}")
        endif()
        if(DEFINED PIXAR_LICENSE_FILE AND NOT "${PIXAR_LICENSE_FILE}" STREQUAL "")
            list(APPEND ALL_TEST_VARS PIXAR_LICENSE_FILE)
            set(MAYAHYDRA_VARNAME_PIXAR_LICENSE_FILE "${PIXAR_LICENSE_FILE}")
        endif()
        # RMAN_SHADERPATH: hdPrman/rmanOslParser needs this to find OSL shaders (UsdPreviewSurfaceParameters.oso, etc.)
        # Artifact layout: plugin/usd/resources/shaders with .oso files
        if(DEFINED PRMAN_DELEGATE_PLUGIN_PATH AND NOT "${PRMAN_DELEGATE_PLUGIN_PATH}" STREQUAL "")
            set(RMAN_SHADERPATH "${PRMAN_DELEGATE_PLUGIN_PATH}/usd/resources/shaders")
            if(DEFINED RMANTREE AND NOT "${RMANTREE}" STREQUAL "")
                # Prepend RenderMan lib/shaders; use platform path separator
                set(RMAN_SHADERPATH "${RMANTREE}/lib/shaders;${RMAN_SHADERPATH}")
            endif()
            list(APPEND ALL_TEST_VARS RMAN_SHADERPATH)
            set(MAYAHYDRA_VARNAME_RMAN_SHADERPATH "${RMAN_SHADERPATH}")
        elseif(DEFINED RMANTREE AND NOT "${RMANTREE}" STREQUAL "")
            # Fallback: only RMANTREE, no delegate shaders
            set(RMAN_SHADERPATH "${RMANTREE}/lib/shaders")
            list(APPEND ALL_TEST_VARS RMAN_SHADERPATH)
            set(MAYAHYDRA_VARNAME_RMAN_SHADERPATH "${RMAN_SHADERPATH}")
        endif()
    else()
        # Explicitly clear PRMan env vars on non-Windows to avoid accidental loads.
        list(APPEND ALL_TEST_VARS PRMAN_DELEGATE_PLUGIN_PATH RMANTREE RENDERMAN_LOCATION PIXAR_LICENSE_FILE RMAN_SHADERPATH)
        set(MAYAHYDRA_VARNAME_PRMAN_DELEGATE_PLUGIN_PATH "")
        set(MAYAHYDRA_VARNAME_RMANTREE "")
        set(MAYAHYDRA_VARNAME_RENDERMAN_LOCATION "")
        set(MAYAHYDRA_VARNAME_PIXAR_LICENSE_FILE "")
        set(MAYAHYDRA_VARNAME_RMAN_SHADERPATH "")
    endif()
    # Escape semicolons in RMAN_SHADERPATH so set_property ENVIRONMENT receives
    # a single value (path1;path2) instead of splitting on CMake list separator.
    if("RMAN_SHADERPATH" IN_LIST ALL_TEST_VARS)
        separate_argument_list(MAYAHYDRA_VARNAME_RMAN_SHADERPATH)
    endif()
    foreach(testvar PRMAN_DELEGATE_PLUGIN_PATH RMANTREE RENDERMAN_LOCATION PIXAR_LICENSE_FILE RMAN_SHADERPATH)
        if("${testvar}" IN_LIST ALL_TEST_VARS)
            set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
                "${testvar}=${MAYAHYDRA_VARNAME_${testvar}}")
        endif()
    endforeach()
    
    # lookdevx
    if(DEFINED LOOKDEVX_LOCATION)
        list(APPEND MAYAHYDRA_VARNAME_PATH
             "${LOOKDEVX_LOCATION}/bin")
        list(APPEND MAYAHYDRA_VARNAME_PATH
             "${LOOKDEVX_LOCATION}/plug-ins")
        list(APPEND MAYAHYDRA_VARNAME_MAYA_SCRIPT_PATH
             "${LOOKDEVX_LOCATION}/scripts") #Contains some AE templates files
        list(APPEND MAYAHYDRA_VARNAME_PYTHONPATH
             "${LOOKDEVX_LOCATION}/scripts")#Contains some python scripts
        list(APPEND MAYAHYDRA_VARNAME_PYTHONPATH
             "${LOOKDEVX_LOCATION}/python")
        list(APPEND MAYAHYDRA_VARNAME_MAYA_PLUG_IN_PATH
             "${LOOKDEVX_LOCATION}/plug-ins")
        list(APPEND MAYAHYDRA_VARNAME_PXR_MTLX_STDLIB_SEARCH_PATHS
             "${LOOKDEVX_LOCATION}/libraries-lookdevx")
        list(APPEND MAYAHYDRA_VARNAME_MATERIALX_SEARCH_PATH
             "${LOOKDEVX_LOCATION}/libraries")
        list(APPEND MAYAHYDRA_VARNAME_MATERIALX_SEARCH_PATH
             "${LOOKDEVX_LOCATION}/libraries-lookdevx")
    endif()

    if(DEFINED BIFROST_LOCATION)
        #The bifrost package contains 2 template files that should have been converted by ecg maya hydra to bifrost.mod and vnn.mod files 
        #These .mod files are used to set the environment for Bifrost and VNN plugins in Maya.
		set(BIFROST_MOD "${BIFROST_LOCATION}/bifrost.mod")
		if(EXISTS "${BIFROST_MOD}")
			message(STATUS "bifrost.mod exists at: ${BIFROST_MOD}")
			list(APPEND MAYAHYDRA_VARNAME_MAYA_MODULE_PATH "${BIFROST_LOCATION}") #Add the common location for bifrost and vnn .mod files to maya mod files
		else()
			message(FATAL_ERROR "Could not find bifrost.mod at: ${BIFROST_MOD}")
        endif()
    endif()

    set(ALL_PATH_VARS
        PATH
        PYTHONPATH
        MAYA_MODULE_PATH
        MAYA_PLUG_IN_PATH
        MAYA_SCRIPT_PATH
        XBMLANGPATH
        ${PXR_OVERRIDE_PLUGINPATH_NAME}
        PXR_MTLX_STDLIB_SEARCH_PATHS
        MATERIALX_SEARCH_PATH
    )

    foreach(pathvar ${ALL_PATH_VARS})
        set(MAYAHYDRA_VARNAME_${pathvar} ${MAYAHYDRA_VARNAME_${pathvar}} PARENT_SCOPE)
    endforeach()

endfunction()

# Adjust PATH and PYTHONPATH to include USD.
function(_mayaHydra_setup_test_USD_paths)
    # These should come last (esp PYTHONPATH, in case another module is overriding
    # with pkgutil)
   if (DEFINED MAYAHYDRA_TO_USD_RELATIVE_PATH)
        set(USD_INSTALL_LOCATION "${CMAKE_INSTALL_PREFIX}/${MAYAHYDRA_TO_USD_RELATIVE_PATH}")
    else()
        set(USD_INSTALL_LOCATION ${PXR_USD_LOCATION})
    endif()
    # Export OpenUSD location for runtime debugging and plugin discovery.
    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
        "PXR_USD_LOCATION=${USD_INSTALL_LOCATION}"
        "USD_INSTALL_LOCATION=${USD_INSTALL_LOCATION}")
    # Inherit any existing PYTHONPATH, but keep it at the end.
    list(APPEND MAYAHYDRA_VARNAME_PYTHONPATH
        "${USD_INSTALL_LOCATION}/lib/python")
    if(IS_WINDOWS)
        list(APPEND MAYAHYDRA_VARNAME_PATH
            "${USD_INSTALL_LOCATION}/bin")
        list(APPEND MAYAHYDRA_VARNAME_PATH
            "${USD_INSTALL_LOCATION}/lib")
    endif()

    set(ALL_PATH_VARS PATH PYTHONPATH)
    foreach(pathvar ${ALL_PATH_VARS})
        set(MAYAHYDRA_VARNAME_${pathvar} ${MAYAHYDRA_VARNAME_${pathvar}} PARENT_SCOPE)
    endforeach()
endfunction()

function(_mayaHydra_setup_test_finalize_env test_name)
    # NOTE: this should come after any setting of PATH/PYTHONPATH so
    #       that our entries will come first.
    # Inherit any existing PATH/PYTHONPATH, but keep it at the end.
    # This is needed (especially for PATH) because we will overwrite
    # both with the values from our list and we need to keep any
    # system entries.
    list(APPEND MAYAHYDRA_VARNAME_PATH $ENV{PATH})
    list(APPEND MAYAHYDRA_VARNAME_PYTHONPATH $ENV{PYTHONPATH})

    # Inherit PXR_PLUGINPATH_NAME and MAYA_PXR_PLUGINPATH_NAME from the
    # configure-time environment so locally-installed Hydra plugins
    # (e.g. HdArnold, HdPrman) are discovered when running tests.
    # On OSX/Linux, filter out PRMan and MtoA/Arnold paths to avoid TfType
    # redefinition errors (UsdSkelImaging* already defined) when the CI machine's
    # Arnold installation sets MAYA_PXR_PLUGINPATH_NAME to include its full USD
    # plugin tree (linked against a different USD build than Maya's).
    # Arnold's Hydra delegate is already added explicitly via MTOA_LOCATION above.
    foreach(_inherit_var PXR_PLUGINPATH_NAME MAYA_PXR_PLUGINPATH_NAME)
        if(DEFINED ENV{${_inherit_var}} AND NOT "$ENV{${_inherit_var}}" STREQUAL "")
            if(IS_WINDOWS)
                list(APPEND MAYAHYDRA_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME} $ENV{${_inherit_var}})
            else()
                set(_path_list "$ENV{${_inherit_var}}")
                string(REPLACE ":" ";" _path_list "${_path_list}")
                foreach(_path ${_path_list})
                    string(TOLOWER "${_path}" _path_lower)
                    if(NOT _path_lower MATCHES "prman|hdprman|renderman|rman|mtoa|arnold")
                        list(APPEND MAYAHYDRA_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME} "${_path}")
                    endif()
                endforeach()
            endif()
        endif()
    endforeach()

    # Maya USD's Plug may read MAYA_PXR_PLUGINPATH_NAME (when built with
    # PXR_OVERRIDE_PLUGINPATH_NAME=MAYA_PXR_PLUGINPATH_NAME). Set it to the same
    # value so HdArnold and other Hydra plugins are discovered regardless.
    list(APPEND ALL_PATH_VARS MAYA_PXR_PLUGINPATH_NAME)
    set(MAYAHYDRA_VARNAME_MAYA_PXR_PLUGINPATH_NAME ${MAYAHYDRA_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME}})

    # convert the internally-processed envs from cmake list
    foreach(pathvar ${ALL_PATH_VARS})
        separate_argument_list(MAYAHYDRA_VARNAME_${pathvar})
    endforeach()

    # prepend the passed-in ENV values - assume these are already
    # separated + escaped
    foreach(name_value_pair ${ARG_ENV})
        mayaUsd_split_head_tail("${name_value_pair}" "=" env_name env_value)
        if(NOT env_name)
            message(FATAL_ERROR "poorly formatted NAME=VALUE pair - name "
                "missing: ${name_value_pair}")
        endif()

        # now either prepend to existing list, or create new
        if("${env_name}" IN_LIST ALL_PATH_VARS)
            if(IS_WINDOWS)
                set(MAYAHYDRA_VARNAME_${env_name}
                    "${env_value}\;${MAYAHYDRA_VARNAME_${env_name}}")
            else()
                set(MAYAHYDRA_VARNAME_${env_name}
                    "${env_value}:${MAYAHYDRA_VARNAME_${env_name}}")
            endif()
        else()
            set("MAYAHYDRA_VARNAME_${env_name}" ${env_value})
            list(APPEND ALL_PATH_VARS "${env_name}")
        endif()
    endforeach()

    # Unset any MAYA_MODULE_PATH as we set all the individual variables
    # so we don't want to conflict with a MayaUsd module.
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT "MAYA_MODULE_PATH=")

    # set all env vars
    foreach(pathvar ${ALL_PATH_VARS})
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "${pathvar}=${MAYAHYDRA_VARNAME_${pathvar}}")
    endforeach()
    
    # Set a temporary folder path for the TMP,TEMP and MAYA_APP_DIR in which the
    # maya profile will be created.
    # Note: replace bad chars in test_name with _.
    string(REGEX REPLACE "[:<>\|]" "_" SANITIZED_TEST_NAME ${test_name})
    set(MAYA_APP_TEMP_DIR "${CMAKE_BINARY_DIR}/test/Temporary/${SANITIZED_TEST_NAME}")
    # Note: ${WORKING_DIR} can point to the source folder, so don't use it
    #       in any env var that will write files (such as MAYA_APP_DIR).
    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
        "TMP=${MAYA_APP_TEMP_DIR}"
        "TEMP=${MAYA_APP_TEMP_DIR}"
        "MAYA_APP_DIR=${MAYA_APP_TEMP_DIR}")
    file(MAKE_DIRECTORY ${MAYA_APP_TEMP_DIR})

    # Set the Python major version in MAYA_PYTHON_VERSION. Maya 2020 and
    # earlier that are Python 2 only will simply ignore it.
    # without "MAYA_NO_STANDALONE_ATEXIT=1", standalone.uninitialize() will
    # set exitcode to 0
    # MAYA_DISABLE_CIP=1  Avoid fatal crash on start-up.
    # MAYA_DISABLE_CER=1  Customer Error Reporting.
    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
        "MAYA_PYTHON_VERSION=${MAYA_PY_VERSION}"
        "MAYA_NO_STANDALONE_ATEXIT=1"
        "MAYA_DEBUG_ENABLE_CRASH_REPORTING=1"
        "MAYA_DEBUG_NO_SAVE_ON_CRASH=1"
        "MAYA_NO_MORE_ASSERT=1"
        "MAYA_DISABLE_CIP=1"
        "MAYA_DISABLE_CER=1")

    if(IS_MACOSX)
        # LC_ALL/LANG: Use UTF-8 locale. Qt requires UTF-8; LC_ALL=C causes Qt to try
        # switching to "UTF-8" which fails on macOS. en_US.UTF-8 is standard on macOS.
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "LC_ALL=en_US.UTF-8"
            "LANG=en_US.UTF-8")
        # Necessary for tests like DiffCore to find python.
        # Do NOT prepend PXR_USD_LOCATION/lib: the standalone OpenUSD ships HdSt/Metal
        # built against a different USD than Maya's bundled one, causing duplicate
        # TF_DEBUG_ENVIRONMENT_SYMBOL registration crashes on every test.
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "DYLD_LIBRARY_PATH=${MAYA_LOCATION}/MacOS:$ENV{DYLD_LIBRARY_PATH}")
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "DYLD_FRAMEWORK_PATH=${MAYA_LOCATION}/Maya.app/Contents/Frameworks")
    endif()

endfunction()

if (OIIO_idiff_BINARY)
    set(IMAGE_DIFF_TOOL ${OIIO_idiff_BINARY} CACHE STRING "Use idiff for image comparison")
endif()
if (OIIO_oiiotool_BINARY)
    set(OIIOTOOL ${OIIO_oiiotool_BINARY} CACHE STRING "Use oiiotool for diff images (--absdiff --maxchan)")
endif()


#
# mayaUsd_add_test( <test_name>
#                   {PYTHON_MODULE <python_module_name> |
#                    PYTHON_COMMAND <python_code> |
#                    PYTHON_SCRIPT <python_script_file> |
#                    COMMAND <cmd> [<cmdarg> ...] }
#                   [NO_STANDALONE_INIT]
#                   [INTERACTIVE]
#                   [ENV <varname>=<varvalue> ...])
#
#   PYTHON_MODULE      - Module to import and test with unittest.main.
#   PYTHON_COMMAND     - Python code to execute; should call sys.exit
#                        with an appropriate exitcode to indicate success
#                        or failure.
#   PYTHON_SCRIPT      - Python script file to execute; should exit with an
#                        appropriate exitcode to indicate success or failure.
#   WORKING_DIRECTORY  - Directory from which the test executable will be called.
#   COMMAND            - Command line to execute as a test
#   NO_STANDALONE_INIT - Only allowable with PYTHON_MODULE or
#                        PYTHON_COMMAND. With those modes, this
#                        command will generally add some boilerplate code
#                        to ensure that maya is initialized and exits
#                        correctly. Use this option to NOT add that code.
#   INTERACTIVE        - Only allowable with PYTHON_SCRIPT.
#                        The test is run using an interactive (non-standalone)
#                        session of Maya, including the UI.
#                        Tests run in this way should finish by calling Maya's
#                        quit command and returning an exit code of 0 for
#                        success or 1 for failure:
#                            cmds.quit(abort=True, exitCode=exitCode)
#   ENV                - Set or append the indicated environment variables;
#                        Since mayaUsd_add_test internally makes changes to
#                        some environment variables, if a value is given
#                        for these variables, it is appended; all other
#                        variables are set exactly as given. The variables
#                        that mayaUsd_add_test manages (and will append) are:
#                            PATH
#                            PYTHONPATH
#                            MAYA_PLUG_IN_PATH
#                            MAYA_SCRIPT_PATH
#                            PXR_PLUGINPATH_NAME
#                            XBMLANGPATH
#                            LD_LIBRARY_PATH
#                        Note that the format of these name/value pairs should
#                        be the same as that used with
#                        `set_property(TEST test_name APPEND PROPERTY ENVIRONMENT ...)`
#                        That means that if the passed in env var is a "list", it
#                        must already be separated by platform-appropriate
#                        path-separators, escaped if needed - ie, ":" on
#                        Linux/MacOS, and "\;" on Windows. Use
#                        separate_argument_list before passing to this func
#                        if you start with a cmake-style list.
#
function(mayaUsd_add_test test_name)    
    # -----------------
    # 1) Arg processing
    # -----------------

    cmake_parse_arguments(ARG
        "NO_STANDALONE_INIT;INTERACTIVE"                                # options
        "PYTHON_MODULE;PYTHON_COMMAND;PYTHON_SCRIPT;WORKING_DIRECTORY"  # one_value keywords
        "COMMAND;ENV"                                                   # multi_value keywords
        ${ARGN}
    )

    # check that they provided one and ONLY 1 of:
    #     PYTHON_MODULE / PYTHON_COMMAND / PYTHON_SCRIPT / COMMAND
    set(NUM_EXCLUSIVE_ITEMS 0)
    foreach(option_name PYTHON_MODULE PYTHON_COMMAND PYTHON_SCRIPT COMMAND)
        if(ARG_${option_name})
            math(EXPR NUM_EXCLUSIVE_ITEMS "${NUM_EXCLUSIVE_ITEMS} + 1")
        endif()
    endforeach()
    if(NOT NUM_EXCLUSIVE_ITEMS EQUAL 1)
        message(FATAL_ERROR "mayaUsd_add_test: must be called with exactly "
            "one of PYTHON_MODULE, PYTHON_COMMAND, PYTHON_SCRIPT, or COMMAND")
    endif()

    if(ARG_NO_STANDALONE_INIT AND NOT (ARG_PYTHON_MODULE
                                          OR ARG_PYTHON_COMMAND))
        message(FATAL_ERROR "mayaUsd_add_test: NO_STANDALONE_INIT may only be "
            "used with PYTHON_MODULE or PYTHON_COMMAND")
    endif()

    if(ARG_INTERACTIVE AND NOT ARG_PYTHON_SCRIPT)
        message(FATAL_ERROR "mayaUsd_add_test: INTERACTIVE may only be "
            "used with PYTHON_SCRIPT")
    endif()

    # set the working_dir
    if(ARG_WORKING_DIRECTORY)
        set(WORKING_DIR ${ARG_WORKING_DIRECTORY})
    else()
        set(WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    # --------------
    # 2) Create test
    # --------------

    set(PYTEST_CODE "")
    if(ARG_PYTHON_MODULE)
        set(MODULE_NAME "${ARG_PYTHON_MODULE}")
        set(PYTEST_CODE "
import sys
from unittest import main
import ${MODULE_NAME}
main(module=${MODULE_NAME})
")
    elseif(ARG_PYTHON_COMMAND)
        set(PYTEST_CODE "${ARG_PYTHON_COMMAND}")
    elseif(ARG_PYTHON_SCRIPT)
        if (ARG_INTERACTIVE)
            if(WIN32)
                set(QUOTE "'")
            else()
                set(QUOTE "\\\"")
            endif()
            set(MEL_PY_EXEC_COMMAND "python(\"\\n\
import os\\n\
import sys\\n\
import time\\n\
import traceback\\n\
file = ${QUOTE}${ARG_PYTHON_SCRIPT}${QUOTE}\\n\
if not os.path.isabs(file):\\n\
    file = os.path.join(${QUOTE}${CMAKE_CURRENT_SOURCE_DIR}${QUOTE}, file)\\n\
openMode = ${QUOTE}rb${QUOTE}\\n\
compileMode = ${QUOTE}exec${QUOTE}\\n\
globals = {${QUOTE}__file__${QUOTE}: file, ${QUOTE}__name__${QUOTE}: ${QUOTE}__main__${QUOTE}}\\n\
try:\\n\
    exec(compile(open(file, openMode).read(), file, compileMode), globals)\\n\
except Exception:\\n\
    sys.__stderr__.write(traceback.format_exc() + os.linesep)\\n\
    sys.__stderr__.flush()\\n\
    sys.__stdout__.flush()\\n\
    # sleep to give the output streams time to finish flushing - otherwise,\\n\
    # os._exit quits so hard + fast, flush may not happen!\\n\
    time.sleep(.1)\\n\
    os._exit(1)\\n\
\")")
            set(COMMAND_CALL ${MAYA_EXECUTABLE} -c ${MEL_PY_EXEC_COMMAND})
        else()
            set(SCRIPT_PATH "${ARG_PYTHON_SCRIPT}")
            if(NOT IS_ABSOLUTE "${SCRIPT_PATH}")
                set(SCRIPT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT_PATH}")
            endif()
        
            set(COMMAND_CALL ${MAYA_PY_EXECUTABLE} ${SCRIPT_PATH})
        endif()
    else()
        set(COMMAND_CALL ${ARG_COMMAND})
    endif()

    if(PYTEST_CODE)
        if(NOT ARG_NO_STANDALONE_INIT)
            # first, indent pycode
            mayaUsd_indent(indented_PYTEST_CODE "${PYTEST_CODE}")
            # then wrap in try/finally, and call maya.standalone.[un]initialize()
            set(PYTEST_CODE "
import maya.standalone
maya.standalone.initialize(name='python')
try:
${indented_PYTEST_CODE}
finally:
    maya.standalone.uninitialize()
"
            )
        endif()

        set(SCRIPT ${CMAKE_BINARY_DIR}/test/Temporary/scripts/runner_${test_name}.py)
        FILE(WRITE ${SCRIPT} "${PYTEST_CODE}")
        set(COMMAND_CALL ${MAYA_PY_EXECUTABLE} ${SCRIPT})
    endif()

    add_test(
        NAME "${test_name}"
        WORKING_DIRECTORY ${WORKING_DIR}
        COMMAND ${COMMAND_CALL}
    )

    # -----------------
    # 3) Set up environ
    # -----------------

    _mayaHydra_setup_test_common_path_vars()
    list(APPEND MAYAHYDRA_VARNAME_PATH "${CMAKE_INSTALL_PREFIX}/lib/gtest")

    _mayaHydra_setup_test_common_defaults("${test_name}")

    # Emulate what the module files for mayaHydra and mayaUsdPlugin would do.
    _mayaHydra_setup_test_plugins()

    if(IS_WINDOWS AND DEFINED ENV{PYTHONHOME})
        # If the environment contains a PYTHONHOME, also set the path to
        # that folder so that we can find the python DLLs.
        list(APPEND MAYAHYDRA_VARNAME_PATH $ENV{PYTHONHOME})
    endif()

    # Prepend our test utilities and the per-test source directory so they win
    # over copies shipped by installed packages (e.g. maya-usd's imageUtils),
    # which were already added by _mayaHydra_setup_test_plugins(). The per-test
    # source directory is inserted last so it ends up first in the list: a test
    # module may shadow a testUtils module, and testUtils in turn shadows the
    # installed copies. Everything else (USD, inherited PYTHONPATH) still comes
    # after, as it is appended later.
    list(INSERT MAYAHYDRA_VARNAME_PYTHONPATH 0 "${MAYA_HYDRA_DIR}/test/testUtils")
    list(INSERT MAYAHYDRA_VARNAME_PYTHONPATH 0 "${CMAKE_CURRENT_SOURCE_DIR}")

    # Adjust PATH and PYTHONPATH to include USD.
    _mayaHydra_setup_test_USD_paths()

    # Set environment variables as test properties.
    _mayaHydra_setup_test_finalize_env("${test_name}")

    if (ARG_INTERACTIVE)
        # Add the "interactive" label to all tests that launch the Maya UI.
        # This allows bypassing them by using the --label-exclude/-LE option to
        # ctest. This is useful when running tests in a headless configuration.
        set_property(TEST "${test_name}" APPEND PROPERTY LABELS interactive)

        # When running via remote desktop this env var is needed for Maya
        # to function correctly. Has no effect when not running remote.
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "MAYA_ALLOW_OPENGL_REMOTE_SESSION=1")

        # Don't want popup when color management fails.
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "MAYA_CM_DISABLE_ERROR_POPUPS=1")
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "MAYA_COLOR_MGT_NO_LOGGING=1")
            
    else()
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "MAYA_IGNORE_DIALOGS=1")
    endif()
endfunction()

#
# mayaHydra_add_cmd_line_render_test( <scene_file_labeled>
#                           [RENDERER <renderer_name>]
#                           [IMAGE_EXTENSION <extension>]
#                           [FAIL <idiff fail value>]
#                           [FAILPERCENT <idiff failpercent value>]
#                           [WORKING_DIRECTORY <dir>]
#                           [RENDERED_IMAGE_SUBDIR <dir>]
#                           [RENDERED_IMAGE_NAME <file_name>]
#                           [RENDERER_ARGS <extra_args>]
#                           [COPY_SCENE]
#                           [ENV <varname>=<varvalue> ...])
#
# Similar to mayaUsd_add_test but uses the Render executable instead of Maya or
# mayapy.  This is to run batch rendering tests that don't require the Maya UI.
#
# The first argument is the Maya scene file to render.  It can be a relative
# path, and it may have appended labels after a | separator. 
#
#   RENDERER           - Name of renderer to be passed to the Render
#                        (default hydraStorm).
#   IMAGE_EXTENSION    - Image file extension, without the dot (default png).
#                        This is appended to the test name.
#   FAIL               - idiff fail value (default 0.01)
#   FAILPERCENT        - idiff failpercent value idiff (default 1.0)
#   WORKING_DIRECTORY  - Directory from which the test executable will be called.
#   RENDERED_IMAGE_SUBDIR - Rendered image root sub-directory (default "projects/default/images")
#   RENDERED_IMAGE_NAME - Rendered image file name (default test name)
#   RENDERER_ARGS      - Additional command line arguments to pass to the
#                        renderer.
#   COPY_SCENE         - If set, copies the scene file to the temporary project
#                        before rendering.
#   ENV                - Set or append the indicated environment variables;
#                        Similar to mayaUsd_add_test, this function manages
#                        the same environment variables.
#
function(mayaHydra_add_cmd_line_render_test SCENE_FILE_LABELED)
    # -----------------
    # 1) Arg processing
    # -----------------

    cmake_parse_arguments(ARG
        "COPY_SCENE"             # Boolean options.
        "RENDERER;SCENE_FILE;WORKING_DIRECTORY;RENDERED_IMAGE_SUBDIR;RENDERED_IMAGE_NAME;IMAGE_EXTENSION;FAIL;FAILPERCENT;RENDERER_ARGS" # one_value keywords
        "ENV"                                    # multi_value keywords
        ${ARGN}
    )

    # set the working_dir
    if(ARG_WORKING_DIRECTORY)
        set(WORKING_DIR ${ARG_WORKING_DIRECTORY})
    else()
        set(WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    # -------------- 
    # 2) Create test
    # --------------

    set(RENDERER "HdStormRendererPlugin")
    if(ARG_RENDERER)
        set(RENDERER "${ARG_RENDERER}")
    endif()
       
    if(ARG_SCENE_FILE_LABELED)
        set(SCENE_FILE_LABELED "${ARG_SCENE_FILE_LABELED}")
    endif()

    # Parse line to extract scene file and labels added with | syntax.
    get_testfile_and_labels(ALL_LABELS SCENE_FILE ${SCENE_FILE_LABELED})
    # Remove extension to define the test name
    mayaUsd_get_unittest_target(test_name ${SCENE_FILE})

    set(IMAGE_EXTENSION "png")
    if(ARG_IMAGE_EXTENSION)
        set(IMAGE_EXTENSION "${ARG_IMAGE_EXTENSION}")
    endif()

    set(FAIL "0.01")
    if(ARG_FAIL)
        set(FAIL "${ARG_FAIL}")
    endif()

    set(FAILPERCENT "1.0")
    if(ARG_FAILPERCENT)
        set(FAILPERCENT "${ARG_FAILPERCENT}")
    endif()

    # Prepend the scene directory to the argument scene file.
    set(SRC_SCENE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/scenes/${SCENE_FILE})
    if(ARG_COPY_SCENE)
        string(REGEX REPLACE "[:<>\|]" "_" SANITIZED_TEST_NAME ${test_name})
        cmake_path(GET SCENE_FILE FILENAME SCENE_FILE_NAME)
        set(SCENE_PATH "${CMAKE_BINARY_DIR}/test/Temporary/${SANITIZED_TEST_NAME}/projects/default/scenes/${SCENE_FILE_NAME}")
    else()
        set(SCENE_PATH ${SRC_SCENE_PATH})
    endif()

    # Our test command is a trivial script that invokes the Render executable
    # to render an image, then invokes idiff to compare the rendered result
    # with a reference image.

    # The command needs to be the name of an executable, without any 
    # arguments, as CMake calls an executable with that string unparsed.

    set(RENDER_ARGS "\"${RENDER_EXECUTABLE}\" -renderer \"${RENDERER}\" ${ARG_RENDERER_ARGS} \"${SCENE_PATH}\"")

    # Replace illegal characters in test_name with _.  Rendered images are
    # written here.
    string(REGEX REPLACE "[:<>\|]" "_" SANITIZED_TEST_NAME ${test_name})
    set(MAYA_APP_TEMP_DIR "${CMAKE_BINARY_DIR}/test/Temporary/${SANITIZED_TEST_NAME}")

    set(RENDERED_IMAGE_ROOT_DIR "${MAYA_APP_TEMP_DIR}")
    set(RENDERED_IMAGE_SUBDIR "projects/default/images")
    if(ARG_RENDERED_IMAGE_SUBDIR)
        set(RENDERED_IMAGE_SUBDIR "${ARG_RENDERED_IMAGE_SUBDIR}")
    endif()

    set(RENDERED_IMAGE_NAME "${SANITIZED_TEST_NAME}")
    if(ARG_RENDERED_IMAGE_NAME)
        set(RENDERED_IMAGE_NAME "${ARG_RENDERED_IMAGE_NAME}")
    endif()

    set(RENDERED_IMAGE_DIR "${RENDERED_IMAGE_ROOT_DIR}/${RENDERED_IMAGE_SUBDIR}")
    set(RENDERED_IMAGE_PATH "${RENDERED_IMAGE_DIR}/${RENDERED_IMAGE_NAME}.${IMAGE_EXTENSION}")
    cmake_path(REPLACE_EXTENSION SRC_SCENE_PATH ".${IMAGE_EXTENSION}" OUTPUT_VARIABLE EXPECTED_IMAGE_PATH)

    # Always use the discovered idiff binary; do not fall back to PATH
    if (IMAGE_DIFF_TOOL)
        set(IDIFF_CMD "${IMAGE_DIFF_TOOL}")
    else()
        message(FATAL_ERROR "idiff binary not discovered. Set IMAGE_DIFF_TOOL (e.g. via OIIO_idiff_BINARY).")
    endif()

    # CMake add_test COMMAND option arguments are unconditionally quoted.  On
    # Windows this prevents the use of the simple cmd shell, as cmd /c works,
    # but cmd "/c" does not: the cmd shell no longer interprets the /c as a
    # flag argument.  Use PowerShell instead.
    # Cross-platform command runner: PowerShell on Windows, POSIX sh elsewhere.
    set(IDIFF_ARGS  "${IDIFF_CMD} -fail ${FAIL} -failpercent ${FAILPERCENT} \"${RENDERED_IMAGE_PATH}\" \"${EXPECTED_IMAGE_PATH}\"")
    if (WIN32)
        set(CMD PowerShell)
		# Windows (PowerShell)
		set(RENDER_ARGS "& \"${RENDER_EXECUTABLE}\" -renderer \"${RENDERER}\" ${ARG_RENDERER_ARGS} \"${SCENE_PATH}\"")
		set(IDIFF_ARGS "& \"${IDIFF_CMD}\" -fail ${FAIL} -failpercent ${FAILPERCENT} \"${RENDERED_IMAGE_PATH}\" \"${EXPECTED_IMAGE_PATH}\"")
        set(RM_ARGS "Remove-Item \"${RENDERED_IMAGE_DIR}/*\" -Recurse -Force -ErrorAction SilentlyContinue")
		set(CMD_ARGS -Command "${RM_ARGS} \; ${RENDER_ARGS} \; if (\$LASTEXITCODE -eq 0) { ${IDIFF_ARGS} } \; exit \$LASTEXITCODE")
    else()
        # Use POSIX shell; '&&' ensures idiff runs only on successful render
        set(CMD /bin/sh)
        set(CMD_ARGS -c "rm -rf ${RENDERED_IMAGE_DIR}/*; ${RENDER_ARGS} && ${IDIFF_ARGS}")
    endif()

    add_test(
        NAME "${test_name}"
        WORKING_DIRECTORY ${WORKING_DIR}
        COMMAND ${CMD} ${CMD_ARGS}
    )

    # -----------------
    # 3) Set up environ
    # -----------------

    _mayaHydra_setup_test_common_path_vars()
    list(APPEND ALL_PATH_VARS MAYA_RENDER_DESC_PATH)

    _mayaHydra_setup_test_common_defaults("${test_name}")
    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
        "MAYA_DEFAULT_SURFACE_SHADER=standardSurface")

    # Emulate what the module files for mayaHydra and mayaUsdPlugin would do.
    _mayaHydra_setup_test_plugins()
    list(APPEND MAYAHYDRA_VARNAME_MAYA_RENDER_DESC_PATH
         "${CMAKE_INSTALL_PREFIX}/renderDesc")

    # Adjust PYTHONPATH to include the path to our Python modules
    list(APPEND MAYAHYDRA_VARNAME_PYTHONPATH "${MAYA_HYDRA_DIR}/scripts")

    # Adjust PATH and PYTHONPATH to include USD.
    _mayaHydra_setup_test_USD_paths()
    
    # Set environment variables as test properties.
    _mayaHydra_setup_test_finalize_env("${test_name}")

    if(ARG_COPY_SCENE)
        configure_file(${SRC_SCENE_PATH} ${SCENE_PATH} COPYONLY)
        cmake_path(REMOVE_EXTENSION SRC_SCENE_PATH OUTPUT_VARIABLE SRC_SCENE_PATH_USD)
        cmake_path(REMOVE_EXTENSION SCENE_PATH OUTPUT_VARIABLE SCENE_PATH_USD)
        configure_file("${SRC_SCENE_PATH_USD}.usda" "${SCENE_PATH_USD}.usda" COPYONLY)
    endif()

    # For render tests, we don't want interactive UI elements
    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
        "MAYA_IGNORE_DIALOGS=1")
        
    # Add a label to identify render tests
    set_property(TEST "${test_name}" APPEND PROPERTY LABELS cmdLineRender)

    apply_labels_to_test("${ALL_LABELS}" ${test_name})

endfunction()
