# Interactive Unit test scripts (launched with maya.exe instead of mayapy.exe)
set(INTERACTIVE_TEST_SCRIPT_FILES
    testImageDiffing.py
    testMtohCommand.py
# Fail as of 22-Feb-2023.  Entered as MAYA-127898.
#    testMtohBasicRender.py
#    testMtohDagChanges.py
    testMeshes.py
    testNamespaces.py
    testVisibility.py
    testRendererSwitching.py
    testSceneBrowser.py
    testStageAddPrim.py
    testTransforms.py
    testNewSceneWithStage.py
    testMayaLights.py
    testUSDLights.py
    testFlowViewportAPI.py
    testStageVariants.py
    cpp/testColorPreferences.py
    cpp/testCppFramework.py
    cpp/testMayaSceneFlattening.py
    cpp/testMayaUsdUfeItems.py
    cpp/testMergingSceneIndex.py
    cpp/testSelectionSceneIndex.py
    cpp/testWireframeSelectionHighlightSceneIndex.py
    cpp/testFlowViewportAPIViewportInformation.py
    cpp/testFlowViewportAPIAddPrims.py
    cpp/testUsdStageLayerMuting.py
    cpp/testFlowViewportAPIFilterPrims.py
    cpp/testPrimInstancing.py
)

# Interactive Unit test scripts for mesh adapter (launched with maya.exe instead of mayapy.exe)
# Test use of mesh adapter code for mesh support, using environment variable.
# As of 13-Jun-2023, if a test in this list is identical to a test in
# TEST_SCRIPT_FILES, it cannot write to an output directory, because running
# both versions of the test in parallel will conflict.
set(INTERACTIVE_TEST_SCRIPT_FILES_MESH_ADAPTER
    testMeshes.py
    cpp/testMeshAdapterTransform.py
)

# Unit test scripts run with mayapy.exe (no UI, so no image diffing and not possible to set Hydra Storm as the renderer)
set(TEST_SCRIPT_FILES
)

foreach(script ${TEST_SCRIPT_FILES})
    mayaUsd_get_unittest_target(target ${script})
    mayaUsd_add_test(${target}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PYTHON_MODULE ${target}
        ENV
            "MAYA_PLUG_IN_PATH=${CMAKE_INSTALL_PREFIX}/lib/maya"
            "LD_LIBRARY_PATH=${ADDITIONAL_LD_LIBRARY_PATH}"
            "IMAGE_DIFF_TOOL=${IMAGE_DIFF_TOOL}"

            # LD_LIBRARY_PATH needs to be set for the idiff executable because its 
            # RPATH is absolute rather than relative to ORIGIN, meaning the RPATH 
            # points to the absolute path on the machine where idiff was built.
            # This absence of relative paths for RPATH comes from OpenImageIO.
            # We introduce a second workaround to avoid Maya using usd's libpng, 
            # because both use incompatible versions of libpng. This is done by 
            # setting LD_LIBRARY_PATH to IDIFF_LD_LIBRARY_PATH only when we run 
            # idiff using Python's subprocess module.
            "IDIFF_LD_LIBRARY_PATH=${ADDITIONAL_LD_LIBRARY_PATH}:${PXR_USD_LOCATION}/lib64:${PXR_USD_LOCATION}/lib"

            # Maya uses a very old version of GLEW, so we need support for
            # pre-loading a newer version from elsewhere.
            "LD_PRELOAD=${ADDITIONAL_LD_PRELOAD}"

            # Fallback to old color management. We will have to investigate
            # and introduce OCIOv2 compatible version of these tests.
            "MAYA_COLOR_MANAGEMENT_SYNCOLOR=1"

            "MAYA_HYDRA_ENABLE_NATIVE_SCENE_INDEX=1"
    )

    # Assign a CTest label to these tests for easy filtering.
    set_property(TEST ${target} APPEND PROPERTY LABELS mayaHydra)
endforeach()

# Use mesh adapter for mesh support instead of MRenderItem. using Interactive (Maya.exe with UI)
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_MESH_ADAPTER})
    mayaUsd_get_unittest_target(target ${script})
    set(target "${target}_meshAdapter")
    mayaUsd_add_test(${target}
        INTERACTIVE
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PYTHON_SCRIPT ${script}
        ENV
            "MAYA_PLUG_IN_PATH=${CMAKE_INSTALL_PREFIX}/lib/maya"
            "LD_LIBRARY_PATH=${ADDITIONAL_LD_LIBRARY_PATH}"

            "LD_PRELOAD=${ADDITIONAL_LD_PRELOAD}"

            "MAYA_COLOR_MANAGEMENT_SYNCOLOR=1"

            "MAYA_HYDRA_USE_MESH_ADAPTER=1"

            "MAYA_HYDRA_ENABLE_NATIVE_SCENE_INDEX=1"
    )

    # Assign a CTest label to these tests for easy filtering.
    set_property(TEST ${target} APPEND PROPERTY LABELS mayaHydraMeshAdapter)
endforeach()

# Use Maya.exe with UI, interactive tests.
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES})
    mayaUsd_get_unittest_target(target ${script})
    mayaUsd_add_test(${target}
        INTERACTIVE
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PYTHON_SCRIPT ${script}
        ENV
            "MAYA_PLUG_IN_PATH=${CMAKE_INSTALL_PREFIX}/lib/maya"
            "LD_LIBRARY_PATH=${ADDITIONAL_LD_LIBRARY_PATH}"
            "IMAGE_DIFF_TOOL=${IMAGE_DIFF_TOOL}"

            # LD_LIBRARY_PATH needs to be set for the idiff executable because its 
            # RPATH is absolute rather than relative to ORIGIN, meaning the RPATH 
            # points to the absolute path on the machine where idiff was built.
            # This absence of relative paths for RPATH comes from OpenImageIO.
            # We introduce a second workaround to avoid Maya using usd's libpng, 
            # because both use incompatible versions of libpng. This is done by 
            # setting LD_LIBRARY_PATH to IDIFF_LD_LIBRARY_PATH only when we run 
            # idiff using Python's subprocess module.
            "IDIFF_LD_LIBRARY_PATH=${ADDITIONAL_LD_LIBRARY_PATH}:${PXR_USD_LOCATION}/lib64:${PXR_USD_LOCATION}/lib"

            # Maya uses a very old version of GLEW, so we need support for
            # pre-loading a newer version from elsewhere.
            "LD_PRELOAD=${ADDITIONAL_LD_PRELOAD}"

            # Fallback to old color management. We will have to investigate
            # and introduce OCIOv2 compatible version of these tests.
            "MAYA_COLOR_MANAGEMENT_SYNCOLOR=1"

            "MAYA_HYDRA_ENABLE_NATIVE_SCENE_INDEX=1"
    )

    # Add a ctest label to these tests for easy filtering.
    set_property(TEST ${target} APPEND PROPERTY LABELS MayaHydraInteractive)
endforeach()

# C++ unit tests
add_subdirectory(cpp)
