set(TARGET_NAME mayaHydraExternalShapeAdapterExample)

add_library(${TARGET_NAME} SHARED)

# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
        mhExternalShapeAdapterExample.cpp
)

# -----------------------------------------------------------------------------
# compiler configuration
# -----------------------------------------------------------------------------
target_compile_definitions(${TARGET_NAME}
    PRIVATE
        $<$<BOOL:${IS_LINUX}>:LINUX>
        MFB_ALT_PACKAGE_NAME="${TARGET_NAME}"
)

mayaHydra_compile_config(${TARGET_NAME})

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME}
    PRIVATE
        mayaHydraLib
)

# -----------------------------------------------------------------------------
# run-time search paths
# -----------------------------------------------------------------------------
if(IS_MACOSX OR IS_LINUX)
    # This library is installed to lib/usd/externalShapeAdapterExample/, so the
    # rpath origin must match that install path (not lib/maya/).
    mayaUsd_init_rpath(rpath "lib/usd/externalShapeAdapterExample")
    if(DEFINED MAYAHYDRA_TO_USD_RELATIVE_PATH)
        mayaUsd_add_rpath(rpath "../../../${MAYAHYDRA_TO_USD_RELATIVE_PATH}/lib")
        if (IS_LINUX)
            mayaUsd_add_rpath(rpath "../../../${MAYAHYDRA_TO_USD_RELATIVE_PATH}/lib64")
        endif()
        if(IS_MACOSX)
            mayaUsd_add_rpath(rpath "../../../../../Maya.app/Contents/MacOS")
        endif()
    elseif(DEFINED PXR_USD_LOCATION)
        mayaUsd_add_rpath(rpath "${PXR_USD_LOCATION}/lib")
    endif()
    # mayaHydraLib, in lib/.
    mayaUsd_add_rpath(rpath "../../")
    mayaUsd_install_rpath(rpath ${TARGET_NAME})
endif()

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
# The library and its plugInfo.json go under lib/usd, which is on USD's plug-in
# path: that is how MayaHydraAdapterRegistry::LoadAllPlugin() finds the adapter.
set(LIBFILENAME ${CMAKE_SHARED_LIBRARY_PREFIX}${TARGET_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
set(PLUG_INFO_PATH "plugInfo.json")
set(PLUG_INFO_LIBRARY_PATH "${LIBFILENAME}")

configure_file(${PLUG_INFO_PATH} ${CMAKE_CURRENT_BINARY_DIR}/${PLUG_INFO_PATH})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PLUG_INFO_PATH}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/usd/externalShapeAdapterExample/resources)

install(TARGETS ${TARGET_NAME}
        LIBRARY
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/usd/externalShapeAdapterExample
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/usd/externalShapeAdapterExample
)

# The Maya plug-in with the node the adapter translates.
install(FILES mhExternalShapeAdapterExampleNode.py
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/maya)

if(IS_WINDOWS)
    if (NOT CODE_COVERAGE)
        install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}>
                DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/usd/externalShapeAdapterExample OPTIONAL)
    endif()
endif()
