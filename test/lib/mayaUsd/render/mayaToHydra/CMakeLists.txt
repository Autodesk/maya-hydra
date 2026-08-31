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
    testOutlineHover.py|skipOnPlatform:osx # Outline selection highlighting is not available on macOS
	#HYDRA-1940 temporarily disable this test which doesn't pass on Maya 2026
    #testMayaLights.py
    testUSDLights.py
    testUVandUDIM.py
	testPurposeRenderTag.py

	#Tests that have a dependency on other plugins
	testMaterialXOnNative.py|depOnPlugins:lookdevx
    testArnoldLights.py|depOnPlugins:mtoa
	testMayaLightingModes.py|depOnPlugins:mtoa
    testLightingRenderDelegates.py|depOnPlugins:prman|skipOnPlatform:osx # HYDRA-2209: re-enable on OSX
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
    cpp/testLegacyHighlightPassRouting.py
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
        # HYDRA-2416: the hidden-objects sub-case drives the Hydra 2.0
        # dirty-locator notification path, which SIGSEGVs while HdArnold (mtoa)
        # is resident.  mtoa is auto-loaded by the runtime and cannot be
        # unloaded on Windows/macOS, so disable the test until HYDRA-2416 is
        # fixed.  The isolate-select behaviour itself passes (all visibility
        # assertions and snapshot comparisons succeed before the crash).
        #testIsolateSelectPerShape.py
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
    testFlowPluginsHierarchicalProperties.py|skipOnPlatform:osx # HYDRA-2209: BasisCurvesLinearDomain fragment errors on OSX
    testBoundingBox.py
	#Disabled tests on OSX
	testDataProducerSelHighlight.py|skipOnPlatform:osx # HYDRA-1127 : refinedWire not working on OSX
)

# Interactive unit test scripts for Hydra Generative Procedurals.
set(INTERACTIVE_TEST_SCRIPT_FILES_ENABLE_GENERATIVE_PROCEDURAL
    cpp/testHydraGenerativeProcedural.py
)

# Every interactive test runs in both selection highlighting modes, so that both
# shipping modes stay covered. The primary run uses outline, the mode the plugin
# ships as its default, and a _legacy counterpart runs the wireframe highlight.
#
# A test only needs its own reference images per mode when its output actually
# depends on the mode: MayaHydraBaseTestCase uses a "<mode>" subfolder when the
# test supplies one and the shared images otherwise. So a mode-independent test
# needs no entry in any list and no second set of images - and if it turns out to
# be mode-sensitive after all, one of its two runs fails against the shared image
# instead of silently losing coverage.
#
# Outline is not offered everywhere: on USD <= 24.11 HgiGL corrupts the prim ids the
# outline compute shader samples, and it is unsupported on macOS. In both cases the
# render global has a single "Legacy Selection" entry, so every primary run is the
# legacy one and there are no _legacy counterparts. Keep this in sync with
# MAYAHYDRA_NO_OUTLINE_SELECTION_HIGHLIGHT in lib/mayaHydra/mayaPlugin/renderGlobals.cpp.
if(USD_VERSION VERSION_GREATER "0.24.11" AND NOT IS_MACOSX)
    set(MAYAHYDRA_OUTLINE_MODE_AVAILABLE TRUE)
else()
    set(MAYAHYDRA_OUTLINE_MODE_AVAILABLE FALSE)
endif()

# These scripts cannot run in outline mode, so their primary run is forced to
# legacy and they get no _legacy counterpart.
set(OUTLINE_MODE_EXCLUDED_TEST_SCRIPT_FILES
    testGeomSubsetsWireframeHighlight.py # TODO: To support when implementing HYDRA-2458 (geometry subsets)
    testNativeInstancingWireframeHighlight.py # TODO: To support when implementing HYDRA-2456 (single instances)
    testPointInstancingWireframeHighlight.py # TODO: To support when implementing HYDRA-2456 (single instances)
    testStageInstanceablePrimsSelHighlight.py # TODO: To support when implementing HYDRA-2456 (single instances)
    # TODO: revisit with HYDRA-2457 (wireframe-on-shaded). Outline skips translating a selected
    # object's wireframe render item until it is actually needed, so toggling wireframeOnShaded adds
    # a new prim in outline mode instead of just dirtying an existing one, as legacy does.
    cpp/testSceneIndexDirtying.py
    # Intentionally excluded: asserts the legacy highlight prim's material binding is blocked.
    # Outline never creates a highlight prim to bind a material to in the first place.
    cpp/testMaterialBlocking.py
    # Intentionally excluded: asserts pass routing of the legacy dormant-wireframe and
    # FlowViewportSelectionHighlights prims specifically, which only exist in legacy mode. The
    # mode-agnostic pass-routing assertions were split out into testPassFiltering, which runs in
    # both modes.
    cpp/testLegacyHighlightPassRouting.py
)

# The ENV entry pinning a primary run's mode, empty when it can use the default.
function(mayaHydra_primary_mode_env out_var test_filename)
    if(MAYAHYDRA_OUTLINE_MODE_AVAILABLE AND
       NOT ${test_filename} IN_LIST OUTLINE_MODE_EXCLUDED_TEST_SCRIPT_FILES)
        set(${out_var} "" PARENT_SCOPE)
    else()
        set(${out_var} "MAYAHYDRA_SELECTION_HIGHLIGHT_MODE=legacy" PARENT_SCOPE)
    endif()
endfunction()

# Use mesh adapter for mesh support instead of MRenderItem. using Interactive (Maya.exe with UI)
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_MESH_ADAPTER})
    get_testfile_and_labels(all_labels test_filename ${script})
    mayaHydra_primary_mode_env(mode_env ${test_filename})
    mayaUsd_get_unittest_target(target ${test_filename})
    set(target "${target}_meshAdapter")
    mayaUsd_add_test(${target}
        INTERACTIVE
        PYTHON_SCRIPT ${test_filename}
        ENV
            "MAYA_HYDRA_USE_MESH_ADAPTER=1"
            "MAYAUSD_DISABLE_VP2_RENDER_DELEGATE=1"
            ${mode_env}
    )

    # Assign a CTest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

# Leave VP2 render delegate enabled and use interactive Maya with UI.
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_ENABLE_VP2_RENDER_DELEGATE})
    get_testfile_and_labels(all_labels test_filename ${script})
    mayaHydra_primary_mode_env(mode_env ${test_filename})
    mayaUsd_get_unittest_target(target ${test_filename})
    mayaUsd_add_test(${target}
        INTERACTIVE
        PYTHON_SCRIPT ${test_filename}
        ENV
            "MAYA_DEFAULT_SURFACE_SHADER=standardSurface"
            ${mode_env}
    )

    # Assign a CTest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

#  Test for Hydra Generative Procedurals using Interactive (Maya.exe with UI)
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_ENABLE_GENERATIVE_PROCEDURAL})
    get_testfile_and_labels(all_labels test_filename ${script})
    mayaHydra_primary_mode_env(mode_env ${test_filename})
    mayaUsd_get_unittest_target(target ${test_filename})
    mayaUsd_add_test(${target}
        INTERACTIVE
        PYTHON_SCRIPT ${test_filename}
        ENV
            "HDGP_INCLUDE_DEFAULT_RESOLVER=1"
            ${mode_env}
    )

    # Assign a CTest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

# Use Maya.exe with UI, interactive tests.
foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES})    
    get_testfile_and_labels(all_labels test_filename ${script})
    mayaHydra_primary_mode_env(mode_env ${test_filename})
    mayaUsd_get_unittest_target(target ${test_filename})
    mayaUsd_add_test(${target}
        INTERACTIVE
        PYTHON_SCRIPT ${test_filename}
        ENV
            "MAYA_DEFAULT_SURFACE_SHADER=standardSurface"
            "MAYAUSD_DISABLE_VP2_RENDER_DELEGATE=1"
            ${mode_env}
    )

    # Add a ctest label to these tests for easy filtering.
    apply_labels_to_test("${all_labels}" ${target})

endforeach()

# The legacy counterpart of each interactive test. A script whose primary run is
# already the legacy one gets none, so nothing is registered twice.
#   name_suffix - appended before "_legacy", to keep the target unique for
#                 scripts registered by more than one configuration.
#   ARGN        - the ENV entries of the configuration being mirrored.
function(mayaHydra_add_legacy_mode_test script name_suffix)
    if(NOT MAYAHYDRA_OUTLINE_MODE_AVAILABLE)
        return()
    endif()
    get_testfile_and_labels(all_labels test_filename ${script})
    if(${test_filename} IN_LIST OUTLINE_MODE_EXCLUDED_TEST_SCRIPT_FILES)
        return()
    endif()
    mayaUsd_get_unittest_target(target ${test_filename})
    set(target "${target}${name_suffix}_legacy")
    mayaUsd_add_test(${target}
        INTERACTIVE
        PYTHON_SCRIPT ${test_filename}
        ENV
            "MAYAHYDRA_SELECTION_HIGHLIGHT_MODE=legacy"
            ${ARGN}
    )
    apply_labels_to_test("${all_labels}" ${target})
endfunction()

foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES})
    mayaHydra_add_legacy_mode_test(${script} ""
        "MAYA_DEFAULT_SURFACE_SHADER=standardSurface"
        "MAYAUSD_DISABLE_VP2_RENDER_DELEGATE=1")
endforeach()

foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_MESH_ADAPTER})
    mayaHydra_add_legacy_mode_test(${script} "_meshAdapter"
        "MAYA_HYDRA_USE_MESH_ADAPTER=1"
        "MAYAUSD_DISABLE_VP2_RENDER_DELEGATE=1")
endforeach()

foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_ENABLE_VP2_RENDER_DELEGATE})
    mayaHydra_add_legacy_mode_test(${script} ""
        "MAYA_DEFAULT_SURFACE_SHADER=standardSurface")
endforeach()

foreach(script ${INTERACTIVE_TEST_SCRIPT_FILES_ENABLE_GENERATIVE_PROCEDURAL})
    mayaHydra_add_legacy_mode_test(${script} ""
        "HDGP_INCLUDE_DEFAULT_RESOLVER=1")
endforeach()

# Standalone Python unit tests for pure test utilities. These exercise logic
# that does not need Maya (e.g. renderManUtils env/platform gating), so they run
# under mayapy via unittest without initializing a standalone Maya session.
set(PYTHON_UNIT_TEST_MODULES
    testRenderManUtils.py
)
foreach(script ${PYTHON_UNIT_TEST_MODULES})
    get_testfile_and_labels(all_labels test_filename ${script})
    mayaUsd_get_unittest_target(target ${test_filename})
    mayaUsd_add_test(${target}
        PYTHON_SCRIPT "${MAYA_HYDRA_DIR}/test/testUtils/${test_filename}"
    )
    apply_labels_to_test("${all_labels}" ${target})
endforeach()

# C++ unit tests
add_subdirectory(cpp)
