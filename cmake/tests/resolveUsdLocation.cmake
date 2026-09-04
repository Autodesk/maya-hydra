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

file(REMOVE_RECURSE "${_test_root}")
