# Interactive Unit test scripts (launched with maya.exe instead of mayapy.exe)
# Unit test can be disabled with ctest labels.
# Adding labels to the unit test follows this convention:
# testMyTestFile.py|depOnPlugins:lookdev,mtoa,newPlugin|skipOnPlatform:[osx,win,lin]
# See accompanying README.md for currently supported list of label mapping.
# only single platform exclusion is supported currently.
# For specifying exclusion based on plugin dependency, look at: test_to_run.json
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
    testStageAddPrim.py
    testTransforms.py
    testRefinement.py
    testMaterialXOnNative.py|depOnPlugins:lookdevx
    testNewSceneWithStage.py
    # To be reenabled after investigation
    #testMayaDisplayModes.py|skipOnPlatform:osx
    testMayaShadingModes.py
    testMayaDisplayLayers.py
    testMayaIsolateSelect.py
    testMayaLights.py
    testUSDLights.py
    testUVandUDIM.py
    testArnoldLights.py|depOnPlugins:mtoa
    testLookThrough.py
    testObjectTemplate.py
    testStandardSurface.py
    testFlowViewportAPI.py
    testStageVariants.py
    testStagePayloadsReferences.py
    testNurbsPrimitives.py
    testCurveTools.py
    testPolygonPrimitives.py
    testFootPrintNode.py
    testBackgroundColor.py
    testGrid.py
    testUsdTextureToggle.py
    # To be reenabled after investigation
    testDataProducerSelHighlight.py|skipOnPlatform:osx
    testPassingNormalsOnMayaNative.py
    testViewportFilters.py
    cpp/testColorPreferences.py
    cpp/testCppFramework.py
    cpp/testDataProducerExample.py
    cpp/testMayaSceneFlattening.py
    cpp/testMayaUsdUfeItems.py
    cpp/testMergingSceneIndex.py
    cpp/testPathInterface.py
    cpp/testPathMapperRegistry.py
    cpp/testPickHandlerRegistry.py
    cpp/testSelectionSceneIndex.py
    cpp/testWireframeSelectionHighlightSceneIndex.py
    cpp/testFlowViewportAPIViewportInformation.py
    cpp/testFlowViewportAPIAddPrims.py
    cpp/testUsdStageLayerMuting.py
    cpp/testFlowViewportAPIFilterPrims.py
    cpp/testSceneCorrectness.py
    cpp/testPrimInstancing.py
    cpp/testPicking.py
    cpp/testUsdPointInstancePicking.py
    cpp/testUsdNativeInstancePicking.py
    cpp/testUsdPickKind.py
    cpp/testNurbsSurfaces.py
    cpp/testPointInstancingWireframeHighlight.py
)

#Add this test only if the MayaUsd_FOUND (so also MAYAUSDAPI_LIBRARY) has been found during compile time.
if(MayaUsd_FOUND)
    list(APPEND INTERACTIVE_TEST_SCRIPT_FILES testMayaUsdAPIUsage.py)
endif(MayaUsd_FOUND)

# Code coverage is not computed for the Hydra scene browser.
if (NOT CODE_COVERAGE)
    list(APPEND INTERACTIVE_TEST_SCRIPT_FILES testSceneBrowser.py)
endif()

# Interactive Unit test scripts for mesh adapter (launched with maya.exe instead of mayapy.exe)
# Test use of mesh adapter code for mesh support, using environment variable.
# As of 13-Jun-2023, if a test in this list is identical to a test in
# INTERACTIVE_TEST_SCRIPT_FILES, it cannot write to an output directory, because running
# both versions of the test in parallel will conflict.
set(INTERACTIVE_TEST_SCRIPT_FILES_MESH_ADAPTER
    testMeshes.py
    cpp/testMeshAdapterTransform.py
)

# Run the following tests with the VP2 render delegate disabled, to ensure 
# that mayaHydra does not rely on computations made by VP2 for its use.
set(INTERACTIVE_TEST_SCRIPT_FILES_DISABLE_VP2_RENDER_DELEGATE
    cpp/testUsdAnim.py
    cpp/testUsdStageFromFile.py
)

# Use mesh adapter for mesh support instead of MRenderItem. using Interactive (Maya.exe with UI)
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_MESH_ADAPTER})
    get_testfile_and_labels(all_labels test_filename ${script})
    mayaUsd_get_unittest_target(target ${test_filename})
    set(target "${target}_meshAdapter")
    mayaUsd_add_test(${target}
        INTERACTIVE
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PYTHON_SCRIPT ${test_filename}
        ENV
            "MAYA_PLUG_IN_PATH=${CMAKE_INSTALL_PREFIX}/lib/maya"
            "LD_LIBRARY_PATH=${ADDITIONAL_LD_LIBRARY_PATH}"

            "LD_PRELOAD=${ADDITIONAL_LD_PRELOAD}"

            "MAYA_HYDRA_USE_MESH_ADAPTER=1"
    )

    # Assign a CTest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

# Disable VP2 render delegate and use interactive Maya with UI.
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_DISABLE_VP2_RENDER_DELEGATE})
    get_testfile_and_labels(all_labels test_filename ${script})
    mayaUsd_get_unittest_target(target ${test_filename})
    mayaUsd_add_test(${target}
        INTERACTIVE
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PYTHON_SCRIPT ${test_filename}
        ENV
            "MAYA_PLUG_IN_PATH=${CMAKE_INSTALL_PREFIX}/lib/maya"
            "LD_LIBRARY_PATH=${ADDITIONAL_LD_LIBRARY_PATH}"

            "LD_PRELOAD=${ADDITIONAL_LD_PRELOAD}"

            "MAYAUSD_DISABLE_VP2_RENDER_DELEGATE=1"
    )

    # Assign a CTest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

# Use Maya.exe with UI, interactive tests.
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES})    
    get_testfile_and_labels(all_labels test_filename ${script})
    mayaUsd_get_unittest_target(target ${test_filename})
    mayaUsd_add_test(${target}
        INTERACTIVE
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PYTHON_SCRIPT ${test_filename}
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
    )

    # Add a ctest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

# C++ unit tests
add_subdirectory(cpp)
