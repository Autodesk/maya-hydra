# -----------------------------------------------------------------------------
# setup
# -----------------------------------------------------------------------------
find_package(GTest REQUIRED)

if (NOT Qt6_FOUND)
    message(WARNING "No Qt6 package found. Cannot build ${TARGET_NAME}.")
    return()
endif()

set(TARGET_NAME mayaHydraCppTests)
add_library(${TARGET_NAME} SHARED)

# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
        mayaHydraCppTestsCmd.cpp
        testColorPreferences.cpp
        testCppFramework.cpp
        testHydraPrim.cpp
        testMayaHydraSceneIndex.cpp
        testMayaSceneFlattening.cpp
        testMayaUsdUfeItems.cpp
        testDataProducerMergingSceneIndex.cpp
        testUsdSelection.cpp
        testPrimPath.cpp
        testSelectionSceneIndex.cpp
        testUtils.cpp
        testFvpViewportInformationMultipleViewports.cpp
        testFvpViewportInformationRendererSwitching.cpp
        testFlowViewportAPIAddPrims.cpp
        testUsdStageLayerMuting.cpp
        testMeshAdapterTransform.cpp
        testCustomAttributeDirtying.cpp
        testFlowViewportAPIFilterPrims.cpp
        testSceneCorrectness.cpp
        testSelection.cpp
        testPrimInstancing.cpp
        testPathMapperRegistry.cpp
        testPickHandlerRegistry.cpp
        testPicking.cpp
        testUsdAnim.cpp
        testUsdPicking.cpp
        testNurbsSurfaces.cpp
        testGeomSubsetsPicking.cpp
        testSinglePicking.cpp
        testSceneIndexDirtying.cpp
        testUsdStageInvalidate.cpp
        testWriteFile.cpp
        testCamera.cpp
        testCustomAttributes.cpp
        testCustomAttributeTypes.cpp
        testMaterialBlocking.cpp
        testPassFiltering.cpp
        testSceneGlobalsCurrentFrame.cpp
)

if (MAYA_HAS_VIEW_SELECTED_OBJECT_API)
    target_sources(${TARGET_NAME}
        PRIVATE
        testIsolateSelect.cpp
    )
endif()

# -----------------------------------------------------------------------------
# compiler configuration
# -----------------------------------------------------------------------------
# QT_NO_KEYWORDS prevents Qt from defining the foreach, signals, slots and emit macros.
# this avoids overlap between Qt macros and boost, and enforces using Q_ macros.
get_target_property(qt_core_aliased Qt6::Core ALIASED_TARGET)
if (qt_core_aliased)
    set_target_properties(${qt_core_aliased} PROPERTIES INTERFACE_COMPILE_DEFINITIONS QT_NO_KEYWORDS)
else()
    set_target_properties(Qt6::Core PROPERTIES INTERFACE_COMPILE_DEFINITIONS QT_NO_KEYWORDS)
endif()

mayaHydra_compile_config(${TARGET_NAME})

target_compile_definitions(${TARGET_NAME}
    PRIVATE
        $<$<BOOL:${IS_MACOSX}>:OSMac_>
        $<$<BOOL:${IS_LINUX}>:LINUX>
        $<$<BOOL:${IS_WINDOWS}>:GTEST_LINKED_AS_SHARED_LIBRARY>
)

# -----------------------------------------------------------------------------
# include directories
# -----------------------------------------------------------------------------
target_include_directories(${TARGET_NAME} 
    PUBLIC
        ${GTEST_INCLUDE_DIRS}
)

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME}
    PUBLIC
        mayaHydraLib
        ${GTEST_LIBRARIES}
        flowViewport
        Qt6::Core
        Qt6::Widgets
)

# -----------------------------------------------------------------------------
# properties
# -----------------------------------------------------------------------------
maya_set_plugin_properties(${TARGET_NAME})

# -----------------------------------------------------------------------------
# run-time search paths
# -----------------------------------------------------------------------------
if(IS_MACOSX OR IS_LINUX) 
    mayaUsd_init_rpath(rpath "lib/maya")
    mayaUsd_add_rpath(rpath "${CMAKE_INSTALL_PREFIX}/lib/gtest")
    mayaUsd_install_rpath(rpath ${TARGET_NAME})
endif()

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(TARGETS ${TARGET_NAME}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/maya)
