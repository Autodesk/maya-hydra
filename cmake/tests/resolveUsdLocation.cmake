include("${CMAKE_CURRENT_LIST_DIR}/../resolveUsdLocation.cmake")

set(IS_MACOSX TRUE)
set(_test_root "${CMAKE_CURRENT_BINARY_DIR}/resolveUsdLocationTest")
set(_maya_usd_location "${_test_root}/mayausd/MayaUSD")
set(_bundled_usd_location "${_test_root}/mayausd/USD")

file(MAKE_DIRECTORY "${_maya_usd_location}")
file(MAKE_DIRECTORY "${_bundled_usd_location}")
file(WRITE "${_bundled_usd_location}/pxrConfig.cmake" "")

mayaHydra_resolve_usd_location(
    _resolved_usd_location
    "${_test_root}/standalone/USD"
    "${_maya_usd_location}"
)

get_filename_component(_expected_usd_location "${_bundled_usd_location}" REALPATH)
if(NOT _resolved_usd_location STREQUAL _expected_usd_location)
    message(FATAL_ERROR
        "Expected MayaUSD's bundled USD at '${_expected_usd_location}', "
        "but resolved '${_resolved_usd_location}'")
endif()

set(_standalone_usd_location "${_test_root}/standalone/USD")
file(REMOVE "${_bundled_usd_location}/pxrConfig.cmake")
mayaHydra_resolve_usd_location(
    _resolved_usd_location
    "${_standalone_usd_location}"
    "${_maya_usd_location}"
)
if(NOT _resolved_usd_location STREQUAL _standalone_usd_location)
    message(FATAL_ERROR
        "Expected standalone USD when MayaUSD has no bundled USD, "
        "but resolved '${_resolved_usd_location}'")
endif()

set(IS_MACOSX FALSE)
file(WRITE "${_bundled_usd_location}/pxrConfig.cmake" "")
mayaHydra_resolve_usd_location(
    _resolved_usd_location
    "${_standalone_usd_location}"
    "${_maya_usd_location}"
)
if(NOT _resolved_usd_location STREQUAL _standalone_usd_location)
    message(FATAL_ERROR
        "Expected standalone USD on non-macOS platforms, "
        "but resolved '${_resolved_usd_location}'")
endif()

set(_standalone_library
    "${_test_root}/standalone/USD/lib/libusd_hdGp.dylib")
set(_bundled_library
    "${_bundled_usd_location}/lib/libusd_hdGp.dylib")
file(MAKE_DIRECTORY "${_test_root}/standalone/USD/lib")
file(MAKE_DIRECTORY "${_bundled_usd_location}/lib")
file(WRITE "${_standalone_library}" "")
file(WRITE "${_bundled_library}" "")
file(WRITE "${_test_root}/CMakeLists.txt" "
cmake_minimum_required(VERSION 3.21)
project(resolveUsdImportedTargets NONE)
include(\"${CMAKE_CURRENT_LIST_DIR}/../resolveUsdLocation.cmake\")
add_library(hdGp SHARED IMPORTED)
set_target_properties(hdGp PROPERTIES
    IMPORTED_CONFIGURATIONS RELWITHDEBINFO
    IMPORTED_LOCATION_RELWITHDEBINFO \"${_standalone_library}\")
mayaHydra_retarget_usd_imported_libraries(\"${_bundled_usd_location}\")
get_target_property(
    _resolved_library hdGp IMPORTED_LOCATION_RELWITHDEBINFO)
if(NOT _resolved_library STREQUAL \"${_bundled_library}\")
    message(FATAL_ERROR
        \"Expected '${_bundled_library}', got '\${_resolved_library}'\")
endif()
")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${_test_root}"
        -B "${_test_root}/build"
    RESULT_VARIABLE _configure_result
    OUTPUT_VARIABLE _configure_output
    ERROR_VARIABLE _configure_error
)
if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR
        "Imported library retargeting test failed:\n"
        "${_configure_output}\n${_configure_error}")
endif()

file(REMOVE_RECURSE "${_test_root}")
