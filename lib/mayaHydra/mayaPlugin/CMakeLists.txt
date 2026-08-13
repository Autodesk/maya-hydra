set(TARGET_NAME mayaHydra)

add_library(${TARGET_NAME} SHARED)

# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME} 
    PRIVATE
        colorNotFoundException.cpp
        envSettings.cpp
        mayaColorPreferencesTranslator.cpp
        plugin.cpp
        pluginDebugCodes.cpp
        pluginUtils.cpp
        renderSettingsUtils.cpp
        renderGlobals.cpp
        renderOverride.cpp
        tokens.cpp
        viewCommand.cpp
        pluginBuildInfoCommand.cpp
        getFramePassesCountCommand.cpp
        testingCommand.cpp
        profilingCommand.cpp
        renderRegionCommand.cpp
        setVisibleFramePassesCommand.cpp
)

if(MAYA_APP_VERSION VERSION_GREATER 2026)
    target_sources(${TARGET_NAME}
        PRIVATE
            batchRendering/batchRenderer.cpp
            batchRendering/batchRendererMayaRenderSettings.cpp
            batchRendering/batchRendererHydraV1RenderSettings.cpp
            batchRendering/batchRendererHydraV2RenderSettings.cpp
            batchRendering/hydraRenderCmd.cpp
            batchRendering/hydraRenderCmdHydraV1RenderSettings.cpp
            batchRendering/hydraRenderCmdHydraV2RenderSettings.cpp
            batchRendering/hydraRenderCmdMayaRenderSettings.cpp
            batchRendering/renderVarUtils.cpp
            batchRendering/tokens.cpp
    )
endif()

set(HEADERS
    pluginUtils.h
    envSettings.h
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList(HEADERS ${HEADERS} SUBDIR render/mayaToHydra)

# -----------------------------------------------------------------------------
# compiler configuration
# -----------------------------------------------------------------------------
target_compile_definitions(${TARGET_NAME}
    PRIVATE 
        # M3dView needs this (technically, MNativeWindowHdl.h)
        # private atm because M3dView is only used in .cpp right now
        # gcc will NOT set this automatically
        $<$<BOOL:${IS_LINUX}>:LINUX>
        # Not sure if msvcc sets this automatically, but won't hurt to redefine
        $<$<BOOL:${IS_WINDOWS}>:_WIN32>
)

if (MAYA_HAS_VIEW_SELECTED_OBJECT_API)
    target_compile_definitions(${TARGET_NAME}
        PRIVATE
            MAYA_HAS_VIEW_SELECTED_OBJECT_API=1
    )
endif()

# -----------------------------------------------------------------------------
# include directories
# -----------------------------------------------------------------------------
target_include_directories(${TARGET_NAME} 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<$<BOOL:${UFE_FOUND}>:${UFE_INCLUDE_DIR}>
        $<$<BOOL:${MayaUsd_FOUND}>:${MAYAUSD_INCLUDE_DIR}>
)

if(MAYA_APP_VERSION VERSION_GREATER 2026)
    target_include_directories(${TARGET_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/batchRendering
    )
endif()

if(DEFINED MAYAUSD_VERSION)
    target_compile_definitions(${TARGET_NAME}
        PRIVATE
            MAYAUSD_VERSION=${MAYAUSD_VERSION}
    )
endif()

if(DEFINED MAYAHYDRA_VERSION)
    target_compile_definitions(${TARGET_NAME}
        PRIVATE
            MAYAHYDRA_VERSION=${MAYAHYDRA_VERSION}
    )
endif()

mayaHydra_compile_config(${TARGET_NAME})

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME} 
    PRIVATE 
        mayaHydraLib
        ufeExtensions
        flowViewport
        hdGp
        $<$<BOOL:${UFE_FOUND}>:${UFE_LIBRARY}>
        $<$<BOOL:${MayaUsd_FOUND}>:${MAYAUSDAPI_LIBRARY}>
)

# -----------------------------------------------------------------------------
# Qt (optional): viewport hover highlighting needs a mouse-move source, provided
# by a Qt event filter on the viewport widget. Qt6 is only present when the Maya
# devkit ships it (see top-level CMakeLists), so this is entirely optional -- the
# plugin builds and runs without it, just without hover highlighting.
# -----------------------------------------------------------------------------
if(Qt6_FOUND)
    target_sources(${TARGET_NAME}
        PRIVATE
            mhHoverEventFilter.cpp
    )
    target_link_libraries(${TARGET_NAME}
        PRIVATE
            Qt6::Core
            Qt6::Widgets
    )
    target_compile_definitions(${TARGET_NAME}
        PRIVATE
            MAYAHYDRA_HAS_QT=1
    )
endif()

# lld-link (Windows/Clang coverage builds) treats UFE inline copy-ctors emitted
# by renderSettingsUtils.cpp as duplicate symbols vs the same ctors exported from
# ufe_0.dll. The duplicates are identical (same inline definition), so /force:multiple
# safely resolves them by keeping the first definition.
if(IS_CLANG AND IS_WINDOWS)
    target_link_options(${TARGET_NAME}
        PRIVATE
            -Wl,/force:multiple
    )
endif()

# -----------------------------------------------------------------------------
# properties
# -----------------------------------------------------------------------------
maya_set_plugin_properties(${TARGET_NAME})

# -----------------------------------------------------------------------------
# run-time search paths
# -----------------------------------------------------------------------------
if(IS_MACOSX OR IS_LINUX) 
    mayaUsd_init_rpath(rpath "lib/maya")
    if(DEFINED MAYAHYDRA_TO_USD_RELATIVE_PATH)
        mayaUsd_add_rpath(rpath "../../${MAYAHYDRA_TO_USD_RELATIVE_PATH}/lib")
    elseif(DEFINED PXR_USD_LOCATION)
        mayaUsd_add_rpath(rpath "${PXR_USD_LOCATION}/lib")
    endif()
    if (IS_LINUX AND DEFINED MAYAHYDRA_TO_USD_RELATIVE_PATH)
        mayaUsd_add_rpath(rpath "../../${MAYAHYDRA_TO_USD_RELATIVE_PATH}/lib64")
    endif()
    if(IS_MACOSX AND DEFINED MAYAHYDRA_TO_USD_RELATIVE_PATH)
        mayaUsd_add_rpath(rpath "../../../../Maya.app/Contents/MacOS")
    endif()
    if(MayaUsd_FOUND)
        #When MayaUsd_FOUND is true, MAYAUSDAPI_LIBRARY exists as it is required. 
        #MAYAUSDAPI_LIBRARY is the full path name of the maya USD API shared library, so get only its directory into MAYAUSDAPI_LIBRARY_PATH
        get_filename_component(MAYAUSDAPI_LIBRARY_PATH "${MAYAUSDAPI_LIBRARY}" DIRECTORY)
        mayaUsd_add_rpath(rpath "${MAYAUSDAPI_LIBRARY_PATH}")
    endif()
    mayaUsd_add_rpath(rpath "../")
    mayaUsd_install_rpath(rpath ${TARGET_NAME})
endif()

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(TARGETS ${TARGET_NAME}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/maya)

install(FILES ${HEADERS}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME}/render/mayaToHydra
)

if(IS_WINDOWS)
    if (NOT CODE_COVERAGE)
        install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> 
                DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/maya OPTIONAL)
    endif()
endif()

# install top level plugInfo.json that includes the configured plugInfo.json
install(CODE
    "file(WRITE \"${CMAKE_CURRENT_BINARY_DIR}/lib/usd/plugInfo.json\" \"{\n    \\\"Includes\\\": [ \\\"*/resources/\\\" ]\n}\")"
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lib/usd/plugInfo.json
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/usd
)
