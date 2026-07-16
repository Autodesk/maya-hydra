# Interactive Unit test scripts (launched with maya.exe instead of mayapy.exe)
# Unit test can be disabled with ctest labels.
# Adding labels to the unit test follows this convention:
# testMyTestFile.py|depOnPlugins:lookdev,mtoa,newPlugin|skipOnPlatform:[osx,win,lin]
# See accompanying README.md for currently supported list of label mapping.
# only single platform exclusion is supported currently.
# For specifying exclusion based on plugin dependency, look at: test_to_run.json
# Tests are run with the VP2 render delegate disabled, to ensure 
# that mayaHydra does not rely on computations made by VP2 for its use.
#
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
    testMayaShadingModes.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testMayaDisplayLayers.py
    testMayaIsolateSelect.py
	#HYDRA-1940 temporarily disable this test which doesn't pass on Maya 2026
    #testMayaLights.py
    testUSDLights.py
    testUVandUDIM.py
	testPurposeRenderTag.py

	#Tests that have a dependency on other plugins
	testMaterialXOnNative.py|depOnPlugins:lookdevx
    testArnoldLights.py|depOnPlugins:mtoa
	testMayaLightingModes.py|depOnPlugins:mtoa
    # Failing, HYDRA-2031.
	# testLightingByRenderDelegate.py|depOnPlugins:mtoa
	testBifrostGraphUpdate.py|depOnPlugins:bifrost
	#End of tests that have a dependency on other plugins
    testLookThrough.py
    testObjectTemplate.py
    testStandardSurface.py
    testOpenPBRSurface.py
    testFlowViewportAPI.py
    testStagePayloadsReferences.py
    testNurbsPrimitives.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testCurveTools.py
    testPolygonPrimitives.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testFootPrintNode.py
    testBackgroundColor.py
    testGrid.py
    testUsdTextureToggle.py
    testUsdStageDefaultLighting.py
    testPassingNormalsOnMayaNative.py
    testMayaComponentsPicking.py|skipOnPlatform:osx # HYDRA-1882 : Crash in HdxOitResolveTask on OSX
    testCustomShadersNode.py
    testMayaDefaultMaterial.py
	testSceneModified.py
    testIsolateSelectWithGeomSubset.py
    testMayaReference.py
    testTexturedMode.py
    testUsdDeleteCamera.py
    testUsdIncludedPurposes.py
    testUsdInheritsSpecializes.py
	#Disabled tests on OSX
    testPointInstancingWireframeHighlight.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testGeomSubsetsWireframeHighlight.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testNativeInstancingWireframeHighlight.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testMeshWireframeHighlight.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testStageInstanceablePrimsSelHighlight.py|skipOnPlatform:osx # HYDRA-1315 : Wire not showing on OSX
	testStageVariants.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
	#HYDRA-1660 re-enable testMayaDisplayModes
	#testMayaDisplayModes.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    testRenderRegion.py
    testConvergence.py
    testFramePasses.py
    testImagePlanes.py
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
    cpp/testFvpDirtyNotifier.py
    cpp/testMayaNodesAttributesDirtying.py|depOnPlugins:mtoa
    cpp/testLightPrimvars.py|depOnPlugins:mtoa
    cpp/testDataSourceNames.py
    cpp/testCustomDagNodeTranslation.py|depOnPlugins:mtoa
    cpp/testCameraPrimvars.py
    cpp/testUsdStageInvalidate.py
    cpp/testCamera.py
    cpp/testMayaNodesAttributes.py|depOnPlugins:mtoa
    cpp/testUsdAnim.py
    cpp/testUsdStageFromFile.py
    cpp/testSceneGlobalsCurrentFrame.py
	#CPP tests disabled on OSX
    cpp/testWriteFile.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
    cpp/testMaterialBlocking.py
    cpp/testPassFiltering.py
    cpp/testRenderItemDirtyLocators.py
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
        testIsolateSelectPerShape.py
        testIsolateSelectMultipleInstances.py|depOnPlugins:bifrost
    )
endif()

# mtoa custom nodes/attributes not supported on Maya 2026 and earlier.
if(MAYA_APP_VERSION VERSION_GREATER 2026)
    list(APPEND INTERACTIVE_TEST_SCRIPT_FILES
        # HYDRA-2415: aiStandIn/aiVolume custom-node teardown crashes Maya with the
        # V2 locator-notification path while HdArnold is active (SIGSEGV / pure virtual).
        #cpp/testArnoldCustomNodes.py|depOnPlugins:mtoa
        cpp/testArnoldCustomAttributesMayaNodes.py|depOnPlugins:mtoa
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
    testTransformMeshAdapter.py
    testMeshAdapterVisibility.py
    cpp/testMeshAdapterTransform.py
    cpp/testMeshPrimvars.py
    cpp/testMeshDirtyLocators.py
    cpp/testMeshGeomSubsets.py
    cpp/testMeshSubdivTags.py
)

# The following tests incorrectly depend on having the VP2 render delegate
# enabled.  We must fix this, to ensure that mayaHydra does not rely
# on computations made by VP2 for its use.  Entered as HYDRA-1961.
set(INTERACTIVE_TEST_SCRIPT_FILES_ENABLE_VP2_RENDER_DELEGATE
	testViewportFilters.py|depOnPlugins:mtoa
    testFlowPluginsHierarchicalProperties.py
    testBoundingBox.py
	#Disabled tests on OSX
	testDataProducerSelHighlight.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
)

# Interactive unit test scripts for Hydra Generative Procedurals.
set(INTERACTIVE_TEST_SCRIPT_FILES_ENABLE_GENERATIVE_PROCEDURAL
    cpp/testHydraGenerativeProcedural.py
)

# Use mesh adapter for mesh support instead of MRenderItem. using Interactive (Maya.exe with UI)
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_MESH_ADAPTER})
    get_testfile_and_labels(all_labels test_filename ${script})
    mayaUsd_get_unittest_target(target ${test_filename})
    set(target "${target}_meshAdapter")
    mayaUsd_add_test(${target}
        INTERACTIVE
        PYTHON_SCRIPT ${test_filename}
        ENV
            "MAYA_HYDRA_USE_MESH_ADAPTER=1"
            "MAYAUSD_DISABLE_VP2_RENDER_DELEGATE=1"
    )

    # Assign a CTest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

# Leave VP2 render delegate enabled and use interactive Maya with UI.
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_ENABLE_VP2_RENDER_DELEGATE})
    get_testfile_and_labels(all_labels test_filename ${script})
    mayaUsd_get_unittest_target(target ${test_filename})
    mayaUsd_add_test(${target}
        INTERACTIVE
        PYTHON_SCRIPT ${test_filename}
        ENV
            "MAYA_DEFAULT_SURFACE_SHADER=standardSurface"
    )

    # Assign a CTest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

#  Test for Hydra Generative Procedurals using Interactive (Maya.exe with UI)
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_ENABLE_GENERATIVE_PROCEDURAL})
    get_testfile_and_labels(all_labels test_filename ${script})
    mayaUsd_get_unittest_target(target ${test_filename})
    mayaUsd_add_test(${target}
        INTERACTIVE
        PYTHON_SCRIPT ${test_filename}
        ENV
            "HDGP_INCLUDE_DEFAULT_RESOLVER=1"
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
        PYTHON_SCRIPT ${test_filename}
        ENV
            "MAYA_DEFAULT_SURFACE_SHADER=standardSurface"
            "MAYAUSD_DISABLE_VP2_RENDER_DELEGATE=1"
    )

    # Add a ctest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

# C++ unit tests
add_subdirectory(cpp)
