#
# -----------------------------------------------------------------------------
# Special interactive test to be able to start Maya with test configurations.
# Can be started with VS Code but won't be part of test suites.
# -----------------------------------------------------------------------------
set(target _Interactive_Maya)
add_test(
    NAME "${target}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${MAYA_EXECUTABLE}
)

set_property(TEST ${target} APPEND PROPERTY DISABLED True)
set_property(TEST ${target} APPEND PROPERTY LABELS Debugging)
set_property(TEST ${target} APPEND PROPERTY ENVIRONMENT "MAYA_MODULE_PATH=${CMAKE_INSTALL_PREFIX}")

# HYDRA-2506 TEMPORARY: drop all viewport tests on macOS to isolate the
# macOS preflight down to the single cmdLineRender test under diagnosis.
# Remove this guard (restoring the unconditional add_subdirectory(mayaUsd))
# once the diagnosis is complete.
if(NOT IS_MACOSX)
    add_subdirectory(mayaUsd)
endif()
if(MAYA_APP_VERSION VERSION_GREATER 2026)
    add_subdirectory(cmdLineRender)
endif()
