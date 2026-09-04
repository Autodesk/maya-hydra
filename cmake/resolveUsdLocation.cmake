# HYDRA-2506: MayaHydra and MayaUSD must use the same OpenUSD runtime on macOS.
# Loading separate copies runs Tf registry initializers twice. This module
# selects MayaUSD's bundled runtime and retargets imported USD libraries to it.
# Keeping this logic separate also allows testing it in CMake script mode.

function(mayaHydra_resolve_usd_location outputVariable pxrUsdLocation mayaUsdLocation)
    set(_resolved_usd_location "${pxrUsdLocation}")

    # Packaged MayaUSD cuts place their OpenUSD dependency beside MayaUSD:
    #     <package>/mayausd/MayaUSD
    #     <package>/mayausd/USD
    #
    # On macOS, linking MayaHydra against another on-disk copy allows dyld to
    # load both copies in one process. Their Tf registry functions then run
    # twice. Prefer MayaUSD's copy when this package layout is available.
    if(IS_MACOSX AND NOT "${mayaUsdLocation}" STREQUAL "")
        get_filename_component(_maya_usd_location "${mayaUsdLocation}" REALPATH)
        get_filename_component(_maya_usd_package_dir "${_maya_usd_location}" DIRECTORY)
        set(_maya_usd_bundled_usd "${_maya_usd_package_dir}/USD")

        if(EXISTS "${_maya_usd_bundled_usd}/pxrConfig.cmake")
            get_filename_component(
                _resolved_usd_location
                "${_maya_usd_bundled_usd}"
                REALPATH
            )
        endif()
    endif()

    set(${outputVariable} "${_resolved_usd_location}" PARENT_SCOPE)
endfunction()

function(mayaHydra_retarget_usd_imported_libraries usdLocation)
    get_property(_imported_targets DIRECTORY PROPERTY IMPORTED_TARGETS)
    set(_retargeted_count 0)

    foreach(_target IN LISTS _imported_targets)
        set(_location_properties IMPORTED_LOCATION)
        get_target_property(_configurations "${_target}" IMPORTED_CONFIGURATIONS)
        if(_configurations)
            foreach(_configuration IN LISTS _configurations)
                string(TOUPPER "${_configuration}" _configuration_upper)
                list(APPEND
                    _location_properties
                    "IMPORTED_LOCATION_${_configuration_upper}"
                )
            endforeach()
        endif()

        foreach(_property IN LISTS _location_properties)
            get_target_property(_location "${_target}" "${_property}")
            if(NOT _location OR _location MATCHES "-NOTFOUND$")
                continue()
            endif()

            get_filename_component(_library_name "${_location}" NAME)
            set(_bundled_location "${usdLocation}/lib/${_library_name}")
            if(EXISTS "${_bundled_location}")
                set_property(
                    TARGET "${_target}"
                    PROPERTY "${_property}" "${_bundled_location}"
                )
                math(EXPR _retargeted_count "${_retargeted_count} + 1")
            endif()
        endforeach()
    endforeach()

    if(_retargeted_count EQUAL 0)
        message(FATAL_ERROR
            "MayaUSD's OpenUSD bundle contains no libraries matching the "
            "imported OpenUSD targets")
    endif()

    message(STATUS
        "Retargeted ${_retargeted_count} OpenUSD imported library locations "
        "to MayaUSD's bundle")
endfunction()
