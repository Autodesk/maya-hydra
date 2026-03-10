set(MAYA_USD_DIR ${CMAKE_CURRENT_SOURCE_DIR})

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
        set(CURRENT_ADDITIONAL_LD_LIBRARY_PATH $ENV{ADDITIONAL_LD_LIBRARY_PATH})
        set(ADDITIONAL_LD_LIBRARY_PATH "${CURRENT_ADDITIONAL_LD_LIBRARY_PATH}:${MAYAUSDAPI_LIBRARY_PATH}")
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

if (OIIO_idiff_BINARY)
    set(IMAGE_DIFF_TOOL ${OIIO_idiff_BINARY} CACHE STRING "Use idiff for image comparison")
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

    cmake_parse_arguments(PREFIX
        "NO_STANDALONE_INIT;INTERACTIVE"                                # options
        "PYTHON_MODULE;PYTHON_COMMAND;PYTHON_SCRIPT;WORKING_DIRECTORY"  # one_value keywords
        "COMMAND;ENV"                                                   # multi_value keywords
        ${ARGN}
    )

    # check that they provided one and ONLY 1 of:
    #     PYTHON_MODULE / PYTHON_COMMAND / PYTHON_SCRIPT / COMMAND
    set(NUM_EXCLUSIVE_ITEMS 0)
    foreach(option_name PYTHON_MODULE PYTHON_COMMAND PYTHON_SCRIPT COMMAND)
        if(PREFIX_${option_name})
            math(EXPR NUM_EXCLUSIVE_ITEMS "${NUM_EXCLUSIVE_ITEMS} + 1")
        endif()
    endforeach()
    if(NOT NUM_EXCLUSIVE_ITEMS EQUAL 1)
        message(FATAL_ERROR "mayaUsd_add_test: must be called with exactly "
            "one of PYTHON_MODULE, PYTHON_COMMAND, PYTHON_SCRIPT, or COMMAND")
    endif()

    if(PREFIX_NO_STANDALONE_INIT AND NOT (PREFIX_PYTHON_MODULE
                                          OR PREFIX_PYTHON_COMMAND))
        message(FATAL_ERROR "mayaUsd_add_test: NO_STANDALONE_INIT may only be "
            "used with PYTHON_MODULE or PYTHON_COMMAND")
    endif()

    if(PREFIX_INTERACTIVE AND NOT PREFIX_PYTHON_SCRIPT)
        message(FATAL_ERROR "mayaUsd_add_test: INTERACTIVE may only be "
            "used with PYTHON_SCRIPT")
    endif()

    # set the working_dir
    if(PREFIX_WORKING_DIRECTORY)
        set(WORKING_DIR ${PREFIX_WORKING_DIRECTORY})
    else()
        set(WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    # --------------
    # 2) Create test
    # --------------

    set(PYTEST_CODE "")
    if(PREFIX_PYTHON_MODULE)
        set(MODULE_NAME "${PREFIX_PYTHON_MODULE}")
        set(PYTEST_CODE "
import sys
from unittest import main
import ${MODULE_NAME}
main(module=${MODULE_NAME})
")
    elseif(PREFIX_PYTHON_COMMAND)
        set(PYTEST_CODE "${PREFIX_PYTHON_COMMAND}")
    elseif(PREFIX_PYTHON_SCRIPT)
        if (PREFIX_INTERACTIVE)
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
file = ${QUOTE}${PREFIX_PYTHON_SCRIPT}${QUOTE}\\n\
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
            set(SCRIPT ${CMAKE_BINARY_DIR}/test/Temporary/scripts/runner_${test_name}.py)
            FILE(WRITE ${SCRIPT} "${PREFIX_PYTHON_SCRIPT}")
            set(COMMAND_CALL ${MAYA_PY_EXECUTABLE} ${SCRIPT})
        endif()
    else()
        set(COMMAND_CALL ${PREFIX_COMMAND})
    endif()

    if(PYTEST_CODE)
        if(NOT PREFIX_NO_STANDALONE_INIT)
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

    set(ALL_PATH_VARS
        PYTHONPATH
        MAYA_MODULE_PATH
        MAYA_PLUG_IN_PATH
        MAYA_SCRIPT_PATH
        XBMLANGPATH
        ${PXR_OVERRIDE_PLUGINPATH_NAME}
        PXR_MTLX_STDLIB_SEARCH_PATHS
    )

    if(IS_WINDOWS)
        # Put path at the front of the list of env vars.
        list(INSERT ALL_PATH_VARS 0
            PATH
        )
    else()
        list(APPEND ALL_PATH_VARS
            LD_LIBRARY_PATH
            IDIFF_LD_LIBRARY_PATH
            LD_PRELOAD
        )
    endif()

    # Set initial empty values for all path vars
    # NOTE - we prefix varnames with "MAYAUSD_VARNAME_" just to make collision
    # with some existing var less likely

    foreach(pathvar ${ALL_PATH_VARS})
        set(MAYAUSD_VARNAME_${pathvar})
    endforeach()

    if(IS_WINDOWS)
        list(APPEND MAYAUSD_VARNAME_PATH "${CMAKE_INSTALL_PREFIX}/lib/gtest")
        list(APPEND MAYAUSD_VARNAME_PATH "${MAYA_LOCATION}/bin")
    else()
        # Set up environment for idiff execution
        set(MAYAUSD_VARNAME_LD_LIBRARY_PATH "${ADDITIONAL_LD_LIBRARY_PATH}")
    
        # LD_LIBRARY_PATH needs to be set for the idiff executable because its 
        # RPATH is absolute rather than relative to ORIGIN, meaning the RPATH 
        # points to the absolute path on the machine where idiff was built.
        # This absence of relative paths for RPATH comes from OpenImageIO.
        # We introduce a second workaround to avoid Maya using usd's libpng, 
        # because both use incompatible versions of libpng. This is done by 
        # setting LD_LIBRARY_PATH to IDIFF_LD_LIBRARY_PATH only when we run 
        # idiff using Python's subprocess module.
        set(MAYAUSD_VARNAME_IDIFF_LD_LIBRARY_PATH "${ADDITIONAL_LD_LIBRARY_PATH}:${PXR_USD_LOCATION}/lib64:${PXR_USD_LOCATION}/lib")
    
        # Maya uses a very old version of GLEW, so we need support for
        # pre-loading a newer version from elsewhere.
        set(MAYAUSD_VARNAME_LD_PRELOAD "${ADDITIONAL_LD_PRELOAD}")
    endif()

    # Set up environment for overall test and Maya defaults. 
    set(ALL_TEST_VARS
        IMAGE_DIFF_TOOL
        MAYA_HAS_RENDER_ITEM_CULL_MODE_API
    )

    set(MAYAUSD_VARNAME_IMAGE_DIFF_TOOL "${IMAGE_DIFF_TOOL}")

    set(MAYAUSD_VARNAME_MAYA_HAS_RENDER_ITEM_CULL_MODE_API "${MAYA_HAS_RENDER_ITEM_CULL_MODE_API}")

    foreach(testvar ${ALL_TEST_VARS})
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "${testvar}=${MAYAUSD_VARNAME_${testvar}}")
    endforeach()

    # Emulate what the module files for mayaHydra and mayaUsdPlugin would do.

    # mayaHydra
    list(APPEND MAYAUSD_VARNAME_PATH
         "${CMAKE_INSTALL_PREFIX}/lib")
    list(APPEND MAYAUSD_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME}
         "${CMAKE_INSTALL_PREFIX}/lib/usd")
    list(APPEND MAYAUSD_VARNAME_MAYA_PLUG_IN_PATH
         "${CMAKE_INSTALL_PREFIX}/lib/maya")
    list(APPEND MAYAUSD_VARNAME_MAYA_SCRIPT_PATH
         "${CMAKE_INSTALL_PREFIX}/scripts")

    # mayaUsdPlugin
    if(DEFINED MAYAUSD_LOCATION)
        list(APPEND MAYAUSD_VARNAME_PATH
             "${MAYAUSD_LOCATION}/lib")
        list(APPEND MAYAUSD_VARNAME_PYTHONPATH
             "${MAYAUSD_LOCATION}/lib/scripts")
        list(APPEND MAYAUSD_VARNAME_MAYA_SCRIPT_PATH
             "${MAYAUSD_LOCATION}/lib/scripts")
        if (IS_LINUX)
            # On Linux the paths in XBMLANGPATH need a /%B at the end.
            list(APPEND MAYAUSD_VARNAME_XBMLANGPATH
                 "${MAYAUSD_LOCATION}/lib/icons/%B")
        else()
            list(APPEND MAYAUSD_VARNAME_XBMLANGPATH
                 "${MAYAUSD_LOCATION}/lib/icons")
        endif()
        list(APPEND MAYAUSD_VARNAME_PYTHONPATH 
             "${MAYAUSD_LOCATION}/lib/python")
        list(APPEND MAYAUSD_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME}
             "${MAYAUSD_LOCATION}/lib/usd")
        list(APPEND MAYAUSD_VARNAME_MAYA_PLUG_IN_PATH
             "${MAYAUSD_LOCATION}/plugin/adsk/plugin")
        list(APPEND MAYAUSD_VARNAME_PYTHONPATH
             "${MAYAUSD_LOCATION}/plugin/adsk/scripts")
        list(APPEND MAYAUSD_VARNAME_MAYA_SCRIPT_PATH
             "${MAYAUSD_LOCATION}/plugin/adsk/scripts")
        list(APPEND MAYAUSD_VARNAME_PXR_MTLX_STDLIB_SEARCH_PATHS
             "${PXR_USD_LOCATION}/libraries")
        list(APPEND MAYAUSD_VARNAME_PXR_MTLX_STDLIB_SEARCH_PATHS
             "${MAYAUSD_LOCATION}/libraries")
    endif()

    # Additional plugin paths (e.g. HdArnold) for tests that need them
    if(ADDITIONAL_PXR_PLUGINPATH_NAME)
        foreach(extra_path ${ADDITIONAL_PXR_PLUGINPATH_NAME})
            list(APPEND MAYAUSD_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME} "${extra_path}")
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
        list(APPEND MAYAUSD_VARNAME_MAYA_MODULE_PATH
             "${MTOA_LOCATION}")
        # Hydra Arnold render delegate plugin path. Try both layouts: newer Arnold
        # uses usd/bundle/<version>, older uses usd/hydra/<version>. Version is USD
        # without "." (e.g. 2511 for USD 0.25.11). Add only the path that contains
        # plugInfo.json.
        if(DEFINED USD_VERSION)
            string(REGEX REPLACE "^0\\.([0-9]+)\\.([0-9]+)$" "\\1\\2" MTOA_USD_VERSION_HYDRA "${USD_VERSION}")
            set(MTOA_HYDRA_BUNDLE "${MTOA_LOCATION}/usd/bundle/${MTOA_USD_VERSION_HYDRA}")
            set(MTOA_HYDRA_LEGACY "${MTOA_LOCATION}/usd/hydra/${MTOA_USD_VERSION_HYDRA}")
            if(EXISTS "${MTOA_HYDRA_BUNDLE}/plugInfo.json")
                list(APPEND MAYAUSD_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME} "${MTOA_HYDRA_BUNDLE}")
            elseif(EXISTS "${MTOA_HYDRA_LEGACY}/plugInfo.json")
                list(APPEND MAYAUSD_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME} "${MTOA_HYDRA_LEGACY}")
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
    if(DEFINED PRMAN_DELEGATE_PLUGIN_PATH AND NOT "${PRMAN_DELEGATE_PLUGIN_PATH}" STREQUAL "")
        list(APPEND MAYAUSD_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME}
             "${PRMAN_DELEGATE_PLUGIN_PATH}")
    endif()
    if(DEFINED RMANTREE AND NOT "${RMANTREE}" STREQUAL "")
        list(APPEND ALL_TEST_VARS RMANTREE)
        set(MAYAUSD_VARNAME_RMANTREE "${RMANTREE}")
        list(APPEND MAYAUSD_VARNAME_PATH "${RMANTREE}/bin")
        list(APPEND MAYAUSD_VARNAME_PATH "${RMANTREE}/lib")
    endif()
    if(DEFINED RENDERMAN_LOCATION AND NOT "${RENDERMAN_LOCATION}" STREQUAL "")
        list(APPEND ALL_TEST_VARS RENDERMAN_LOCATION)
        set(MAYAUSD_VARNAME_RENDERMAN_LOCATION "${RENDERMAN_LOCATION}")
    endif()
    if(DEFINED PIXAR_LICENSE_FILE AND NOT "${PIXAR_LICENSE_FILE}" STREQUAL "")
        list(APPEND ALL_TEST_VARS PIXAR_LICENSE_FILE)
        set(MAYAUSD_VARNAME_PIXAR_LICENSE_FILE "${PIXAR_LICENSE_FILE}")
    endif()
    foreach(testvar RMANTREE RENDERMAN_LOCATION PIXAR_LICENSE_FILE)
        if("${testvar}" IN_LIST ALL_TEST_VARS)
            set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
                "${testvar}=${MAYAUSD_VARNAME_${testvar}}")
        endif()
    endforeach()
    
    # lookdevx
    if(DEFINED LOOKDEVX_LOCATION)
        list(APPEND MAYAUSD_VARNAME_PATH
             "${LOOKDEVX_LOCATION}/bin")
        list(APPEND MAYAUSD_VARNAME_PATH
             "${LOOKDEVX_LOCATION}/plug-ins")
        list(APPEND MAYAUSD_VARNAME_MAYA_SCRIPT_PATH
             "${LOOKDEVX_LOCATION}/scripts") #Contains some AE templates files
        list(APPEND MAYAUSD_VARNAME_PYTHONPATH
             "${LOOKDEVX_LOCATION}/scripts")#Contains some python scripts
        list(APPEND MAYAUSD_VARNAME_PYTHONPATH
             "${LOOKDEVX_LOCATION}/python")
        list(APPEND MAYAUSD_VARNAME_MAYA_PLUG_IN_PATH
             "${LOOKDEVX_LOCATION}/plug-ins")
    endif()

    if(DEFINED BIFROST_LOCATION)
        #The bifrost package contains 2 template files that should have been converted by ecg maya hydra to bifrost.mod and vnn.mod files 
        #These .mod files are used to set the environment for Bifrost and VNN plugins in Maya.
		set(BIFROST_MOD "${BIFROST_LOCATION}/bifrost.mod")
		if(EXISTS "${BIFROST_MOD}")
			message(STATUS "bifrost.mod exists at: ${BIFROST_MOD}")
			list(APPEND MAYAUSD_VARNAME_MAYA_MODULE_PATH "${BIFROST_LOCATION}") #Add the common location for bifrost and vnn .mod files to maya mod files
		else()
			message(FATAL_ERROR "Could not find bifrost.mod at: ${BIFROST_MOD}")
        endif()
    endif()

    if(IS_WINDOWS AND DEFINED ENV{PYTHONHOME})
        # If the environment contains a PYTHONHOME, also set the path to
        # that folder so that we can find the python DLLs.
        list(APPEND MAYAUSD_VARNAME_PATH $ENV{PYTHONHOME})
    endif()

    # Adjust PYTHONPATH to include the path to our test utilities.
    list(APPEND MAYAUSD_VARNAME_PYTHONPATH "${MAYA_USD_DIR}/test/testUtils")

    # Adjust PYTHONPATH to include the path to our test.
    list(APPEND MAYAUSD_VARNAME_PYTHONPATH "${CMAKE_CURRENT_SOURCE_DIR}")

    # Adjust PATH and PYTHONPATH to include USD.
    # These should come last (esp PYTHONPATH, in case another module is overriding
    # with pkgutil)
   if (DEFINED MAYAHYDRA_TO_USD_RELATIVE_PATH)
        set(USD_INSTALL_LOCATION "${CMAKE_INSTALL_PREFIX}/${MAYAHYDRA_TO_USD_RELATIVE_PATH}")
    else()
        set(USD_INSTALL_LOCATION ${PXR_USD_LOCATION})
    endif()
    # Inherit any existing PYTHONPATH, but keep it at the end.
    list(APPEND MAYAUSD_VARNAME_PYTHONPATH
        "${USD_INSTALL_LOCATION}/lib/python")
    if(IS_WINDOWS)
        list(APPEND MAYAUSD_VARNAME_PATH
            "${USD_INSTALL_LOCATION}/bin")
        list(APPEND MAYAUSD_VARNAME_PATH
            "${USD_INSTALL_LOCATION}/lib")
    endif()

    # NOTE: this should come after any setting of PATH/PYTHONPATH so
    #       that our entries will come first.
    # Inherit any existing PATH/PYTHONPATH, but keep it at the end.
    # This is needed (especially for PATH) because we will overwrite
    # both with the values from our list and we need to keep any
    # system entries.
    list(APPEND MAYAUSD_VARNAME_PATH $ENV{PATH})
    list(APPEND MAYAUSD_VARNAME_PYTHONPATH $ENV{PYTHONPATH})

    # Inherit PXR_PLUGINPATH_NAME and MAYA_PXR_PLUGINPATH_NAME from the
    # configure-time environment so locally-installed Hydra plugins
    # (e.g. HdArnold, HdPrman) are discovered when running tests.
    if(DEFINED ENV{PXR_PLUGINPATH_NAME})
        list(APPEND MAYAUSD_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME} $ENV{PXR_PLUGINPATH_NAME})
    endif()
    if(DEFINED ENV{MAYA_PXR_PLUGINPATH_NAME})
        list(APPEND MAYAUSD_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME} $ENV{MAYA_PXR_PLUGINPATH_NAME})
    endif()

    # Maya USD's Plug may read MAYA_PXR_PLUGINPATH_NAME (when built with
    # PXR_OVERRIDE_PLUGINPATH_NAME=MAYA_PXR_PLUGINPATH_NAME). Set it to the same
    # value so HdArnold and other Hydra plugins are discovered regardless.
    list(APPEND ALL_PATH_VARS MAYA_PXR_PLUGINPATH_NAME)
    set(MAYAUSD_VARNAME_MAYA_PXR_PLUGINPATH_NAME ${MAYAUSD_VARNAME_${PXR_OVERRIDE_PLUGINPATH_NAME}})

    # convert the internally-processed envs from cmake list
    foreach(pathvar ${ALL_PATH_VARS})
        separate_argument_list(MAYAUSD_VARNAME_${pathvar})
    endforeach()

    # prepend the passed-in ENV values - assume these are already
    # separated + escaped
    foreach(name_value_pair ${PREFIX_ENV})
        mayaUsd_split_head_tail("${name_value_pair}" "=" env_name env_value)
        if(NOT env_name)
            message(FATAL_ERROR "poorly formatted NAME=VALUE pair - name "
                "missing: ${name_value_pair}")
        endif()

        # now either prepend to existing list, or create new
        if("${env_name}" IN_LIST ALL_PATH_VARS)
            if(IS_WINDOWS)
                set(MAYAUSD_VARNAME_${env_name}
                    "${env_value}\;${MAYAUSD_VARNAME_${env_name}}")
            else()
                set(MAYAUSD_VARNAME_${env_name}
                    "${env_value}:${MAYAUSD_VARNAME_${env_name}}")
            endif()
        else()
            set("MAYAUSD_VARNAME_${env_name}" ${env_value})
            list(APPEND ALL_PATH_VARS "${env_name}")
        endif()
    endforeach()

    # Unset any MAYA_MODULE_PATH as we set all the individual variables
    # so we don't want to conflict with a MayaUsd module.
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT "MAYA_MODULE_PATH=")

    # set all env vars
    foreach(pathvar ${ALL_PATH_VARS})
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "${pathvar}=${MAYAUSD_VARNAME_${pathvar}}")
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
        # LC_ALL/LANG: Avoid locale.Error: unsupported locale setting on macOS CI
        # when LANG/LC_* inherit a value (e.g. en_US.UTF-8) not installed on the agent.
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "LC_ALL=C"
            "LANG=C")
        # Necessary for tests like DiffCore to find python
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "DYLD_LIBRARY_PATH=${MAYA_LOCATION}/MacOS:$ENV{DYLD_LIBRARY_PATH}")
        set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
            "DYLD_FRAMEWORK_PATH=${MAYA_LOCATION}/Maya.app/Contents/Frameworks")
    endif()

    if (PREFIX_INTERACTIVE)
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
