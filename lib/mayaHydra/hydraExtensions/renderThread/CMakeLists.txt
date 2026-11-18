# -----------------------------------------------------------------------------
# sources
# -----------------------------------------------------------------------------
target_sources(${TARGET_NAME}
    PRIVATE
        renderThread.cpp
        renderThreadContext.cpp
        renderThreadFence.cpp
        renderThreadPresentBridge.cpp
        renderThreadStats.cpp
)

set(HEADERS
    renderThread.h
    renderThreadContext.h
    renderThreadFence.h
    renderThreadHandoff.h
    renderThreadPresentBridge.h
    renderThreadStats.h
)

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME}
    PRIVATE
        hgiInterop
        garch
        # The Vulkan AOV copy reaches hgiVulkan directly; the USD_HAS_HGI_VULKAN
        # definition that compiles it in is set on this target by its parent.
        $<$<BOOL:${USD_HAS_HGI_VULKAN}>:hgiVulkan>
        # The render thread's autorelease pools call the Objective-C runtime.
        $<$<PLATFORM_ID:Darwin>:objc>
)

# -----------------------------------------------------------------------------
# promoted headers
# -----------------------------------------------------------------------------
mayaUsd_promoteHeaderList(
    HEADERS
        ${HEADERS}
    BASEDIR
        ${TARGET_NAME}/renderThread
)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(FILES ${HEADERS}
    DESTINATION
        ${CMAKE_INSTALL_PREFIX}/include/mayaHydraLib/renderThread
)
