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

add_subdirectory(mayaUsd)
