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
