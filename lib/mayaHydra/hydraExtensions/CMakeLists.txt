set(TARGET_NAME mayaHydraLib)

add_library(${TARGET_NAME} SHARED)

# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME} 
    PRIVATE
        debugCodes.cpp
        hydraUtils.cpp
        mayaHydraLibInterfaceImp.cpp
        mayaUtils.cpp
        mhBuildInfo.cpp
        mixedUtils.cpp
        mhWireframeColorInterfaceImp.cpp
        mhLeadObjectPathTracker.cpp
        tokens.cpp
		mhDataProducersMayaNodeToSdfPathRegistry.cpp
)

set(HEADERS
    api.h
    debugCodes.h
    hydraUtils.h
    mayaHydraParams.h
    mayaHydraLibInterface.h
    mayaHydraLibInterfaceImp.h
    mayaUtils.h
    mixedUtils.h
    mhWireframeColorInterfaceImp.h
    mhLeadObjectPathTracker.h
    tokens.h
	mhDataProducersMayaNodeToSdfPathRegistry.h
)

# -----------------------------------------------------------------------------
# compiler configuration
# -----------------------------------------------------------------------------
target_compile_definitions(${TARGET_NAME} 
    PRIVATE 
        MAYAHYDRALIB_EXPORT
        $<$<BOOL:${IS_MACOSX}>:OSMac_>
        # Copy-pasted from lib/mayaUsd/CMakeLists.txt
)
        
mayaHydra_compile_config(${TARGET_NAME})

# -----------------------------------------------------------------------------
# include directories
# -----------------------------------------------------------------------------
target_include_directories(${TARGET_NAME} 
            SYSTEM PUBLIC
                ${PXR_INCLUDE_DIRS}
                ${MAYA_INCLUDE_DIRS}
)
target_include_directories(${TARGET_NAME}
            PUBLIC
                ${PYTHON_INCLUDE_DIR}
                ${BOOST_INCLUDE_DIR}               
                ${CMAKE_BINARY_DIR}/include
            PRIVATE
                $<$<BOOL:${UFE_FOUND}>:${UFE_INCLUDE_DIR}>
                $<$<BOOL:${MayaUsd_FOUND}>:${MAYAUSD_INCLUDE_DIR}>
)

target_compile_definitions(${TARGET_NAME}
        PUBLIC
            PXR_PLUGINPATH_NAME=${PXR_OVERRIDE_PLUGINPATH_NAME}
        PRIVATE
            MAYAUSD_MACROS_EXPORT
            MAYAUSD_CORE_EXPORT
            MFB_PACKAGE_NAME="${PROJECT_NAME}"
            MFB_ALT_PACKAGE_NAME="${PROJECT_NAME}"
            MFB_PACKAGE_MODULE="${PROJECT_NAME}"
            $<$<BOOL:${IS_MACOSX}>:OSMac_>
            $<$<BOOL:${IS_LINUX}>:LINUX>
            $<$<BOOL:${IS_LINUX}>:GL_GLEXT_PROTOTYPES>
            $<$<BOOL:${IS_LINUX}>:GLX_GLXEXT_PROTOTYPES>
            $<$<BOOL:${CMAKE_WANT_MATERIALX_BUILD}>:WANT_MATERIALX_BUILD>
            $<$<BOOL:${BUILD_MAYAHYDRALIB}>:BUILD_MAYAHYDRALIB>

            # this flag is needed when building maya-usd in Maya
            $<$<BOOL:${IS_WINDOWS}>:WIN32>
)

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

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME} 
    PUBLIC
        ar
        gf
        hd
        hdx
        js
        kind
        plug
        sdf
        tf
        trace
        usd
        usdGeom
        usdImaging
        usdImagingGL
        usdLux
        usdRender
        usdRi
        usdShade
        usdSkel
        usdUtils
        ufeExtensions
        flowViewport
        ${MAYA_LIBRARIES}
    PRIVATE
        $<$<BOOL:${USD_001905_BUILD}>:hio>
        ${PYTHON_LIBRARIES} 
        $<$<BOOL:${UFE_FOUND}>:${UFE_LIBRARY}>
        $<$<BOOL:${MayaUsd_FOUND}>:${MAYAUSDAPI_LIBRARY}>
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
set(SRCFILE ${CMAKE_CURRENT_SOURCE_DIR}/mayaHydra.h.src)
set(DSTFILE ${CMAKE_BINARY_DIR}/include/mayaHydraLib/mayaHydra.h)
configure_file(${SRCFILE} ${DSTFILE})

set(SRCFILE ${CMAKE_CURRENT_SOURCE_DIR}/mhBuildInfo.h.src)
set(DSTFILE ${CMAKE_BINARY_DIR}/include/mayaHydraLib/mhBuildInfo.h)
configure_file(${SRCFILE} ${DSTFILE})

mayaUsd_promoteHeaderList( 
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
set(LIBFILENAME ${CMAKE_SHARED_LIBRARY_PREFIX}${TARGET_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
set(PLUG_INFO_PATH "plugInfo.json")
set(SHADER_DEFS_PATH 
"shaderDefs.usda"
)

set(PLUG_INFO_LIBRARY_PATH "../../../${LIBFILENAME}")
set(PLUG_INFO_RESOURCE_PATH "resources")

configure_file(${PLUG_INFO_PATH} ${CMAKE_CURRENT_BINARY_DIR}/${PLUG_INFO_PATH})

# configure shader files
foreach(X ${SHADER_DEFS_PATH})
    configure_file(${X} ${CMAKE_CURRENT_BINARY_DIR}/${X})
endforeach(X)


install(TARGETS ${TARGET_NAME}
        LIBRARY
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

install(FILES 
            ${CMAKE_CURRENT_BINARY_DIR}/${PLUG_INFO_PATH}
        DESTINATION 
            ${CMAKE_INSTALL_PREFIX}/lib/usd/mayaHydraLib/resources
)

# install shaders
foreach(X ${SHADER_DEFS_PATH})
    install(FILES 
            ${CMAKE_CURRENT_BINARY_DIR}/${X}
        DESTINATION 
            ${CMAKE_INSTALL_PREFIX}/lib/usd/mayaHydraLib/resources
    )
endforeach(X)


install(FILES ${HEADERS}
        DESTINATION 
            ${CMAKE_INSTALL_PREFIX}/include/mayaHydraLib
)

if(IS_WINDOWS)
    if (NOT CODE_COVERAGE)
        install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> 
                DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)
    endif()
endif()

# -----------------------------------------------------------------------------
# subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(adapters)
add_subdirectory(pick)
add_subdirectory(sceneIndex)
