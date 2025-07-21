# Interactive Unit test scripts (launched with maya.exe instead of mayapy.exe)
# Unit test can be disabled with ctest labels.
# Adding labels to the unit test follows this convention:
# testMyTestFile.py|depOnPlugins:lookdev,mtoa,newPlugin|skipOnPlatform:[osx,win,lin]
# See accompanying README.md for currently supported list of label mapping.
# only single platform exclusion is supported currently.
# For specifying exclusion based on plugin dependency, look at: test_to_run.json
set(INTERACTIVE_TEST_SCRIPT_FILES
    testSceneStat.py
    testImageDiffing.py
    testMtohCommand.py
    testBasicRender.py
    testDagChanges.py
    testMeshes.py
    testNamespaces.py
    testVisibility.py
    testRendererSwitching.py
    testSelectPrimWithoutDataSource.py
    testStageAddPrim.py
    testTransforms.py
    testRefinement.py
	testNewSceneWithStage.py
    testMayaShadingModes.py
    testMayaDisplayLayers.py
    testMayaIsolateSelect.py
    testMayaLights.py
    testUSDLights.py
    testUVandUDIM.py
	#Tests that have a dependency on other plugins
	testMaterialXOnNative.py|depOnPlugins:lookdevx
    testArnoldLights.py|depOnPlugins:mtoa
	testViewportFilters.py|depOnPlugins:mtoa
	testMayaLightingModes.py|depOnPlugins:mtoa
	testBifrostGraphUpdate.py|depOnPlugins:bifrost
	#End of tests that have a dependency on other plugins
    testLookThrough.py
    testObjectTemplate.py
    testStandardSurface.py
    testOpenPBRSurface.py
    testFlowViewportAPI.py
    testStagePayloadsReferences.py
    testNurbsPrimitives.py
    testCurveTools.py
    testPolygonPrimitives.py
    testFootPrintNode.py
    testBackgroundColor.py
    testGrid.py
    testUsdTextureToggle.py
    testUsdStageDefaultLighting.py
    testPassingNormalsOnMayaNative.py
    testMayaComponentsPicking.py
    testFlowPluginsHierarchicalProperties.py
    testCustomShadersNode.py
    testMayaDefaultMaterial.py
	testSceneModified.py
    testIsolateSelectWithGeomSubset.py
    testMayaReference.py
    testUsdInheritsSpecializes.py
    testBoundingBox.py
	#Disabled tests on OSX
	testDataProducerSelHighlight.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testPointInstancingWireframeHighlight.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testGeomSubsetsWireframeHighlight.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testNativeInstancingWireframeHighlight.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testMeshWireframeHighlight.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testStageInstanceablePrimsSelHighlight.py|skipOnPlatform:osx # HYDRA-1315 : Wire not showing on OSX
	testStageVariants.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
	#HYDRA-1660 re-enable testMayaDisplayModes
	#testMayaDisplayModes.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
	#CPP tests
    cpp/testColorPreferences.py
    cpp/testCppFramework.py
    cpp/testDataProducerExample.py
    cpp/testMayaHydraSceneIndex.py
    cpp/testMayaSceneFlattening.py
    cpp/testMayaUsdUfeItems.py
    cpp/testDataProducerMergingSceneIndex.py
    cpp/testUsdSelection.py
    cpp/testPathMapperRegistry.py
    cpp/testPickHandlerRegistry.py
    cpp/testSelectionSceneIndex.py
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
    cpp/testGeomSubsetsPicking.py
    cpp/testSinglePicking.py
    cpp/testSceneIndexDirtying.py
    cpp/testUsdStageInvalidate.py
	#CPP tests disabled on OSX
    cpp/testWriteFile.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
)

# These two test files are identical, except for disabled tests.  See
# HYDRA-1245.
if (MAYA_HAS_VIEW_SELECTED_OBJECT_API)
    list(APPEND INTERACTIVE_TEST_SCRIPT_FILES
        testIsolateSelectWithUsdLighting.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
        cpp/testIsolateSelect.py
        cpp/testIsolateSelectSwitchToVP2.py
        cpp/testIsolateSelectMayaSelectionHighlighting.py
        cpp/testUsdNativeInstancingIsolateSelect.py
        cpp/testUsdPointInstancingIsolateSelect.py
        cpp/testUsdPointInstancingIsolateSelectBBox.py
    )
endif()

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

            "MAYA_HAS_RENDER_ITEM_CULL_MODE_API=${MAYA_HAS_RENDER_ITEM_CULL_MODE_API}"
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

            # Use standard surface as default for now
            "MAYA_DEFAULT_SURFACE_SHADER=standardSurface"

            "MAYA_HAS_RENDER_ITEM_CULL_MODE_API=${MAYA_HAS_RENDER_ITEM_CULL_MODE_API}"
    )

    # Add a ctest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

# C++ unit tests
add_subdirectory(cpp)
