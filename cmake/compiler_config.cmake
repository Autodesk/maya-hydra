#------------------------------------------------------------------------------
# compiler flags/definitions
#------------------------------------------------------------------------------
set(GNU_CLANG_FLAGS
    # we want to be as strict as possible
    -Wall
    $<$<BOOL:${BUILD_STRICT_MODE}>:-Werror>
    $<$<CONFIG:DEBUG>:-fstack-check>
    # optimization
    -msse4.2
    # disable warnings
    -Wno-deprecated
    -Wno-deprecated-declarations
    -Wno-unused-local-typedefs
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND GNU_CLANG_FLAGS
            -Wrange-loop-analysis
        )
endif()

set(MSVC_FLAGS
    # we want to be as strict as possible
    /W3
    $<$<BOOL:${BUILD_STRICT_MODE}>:/WX>
    # enable two-phase name lookup and other strict checks (binding a non-const reference to a temporary, etc..)
    $<$<BOOL:$<VERSION_GREATER:${USD_BOOST_VERSION},106600>>:/permissive->
    # enable pdb generation.
    /Zi
    # standards compliant.
    /Zc:rvalueCast
    # The /Zc:inline option strips out the "arch_ctor_<name>" symbols used for
    # library initialization by ARCH_CONSTRUCTOR starting in Visual Studio 2019,
    # causing release builds to fail. Disable the option for this and later
    # versions.
    #
    # For more details, see:
    # https://developercommunity.visualstudio.com/content/problem/914943/zcinline-removes-extern-symbols-inside-anonymous-n.html
    $<IF:$<VERSION_GREATER_EQUAL:${MSVC_VERSION},1920>,/Zc:inline-,/Zc:inline>
    # enable multiprocessor builds.
    /MP
    # enable exception handling.
    /EHsc
    # enable initialization order as a level 3 warning
    /w35038
    # disable warnings
    /wd4244
    /wd4267
    /wd4273
    /wd4305
    /wd4506
    /wd4996
    /wd4180
    # exporting STL classes
    /wd4251
    # Set some warnings as errors (to make it similar to Linux)
    /we4101
    /we4189
)

set(MSVC_DEFINITIONS
    # Make sure WinDef.h does not define min and max macros which
    # will conflict with std::min() and std::max().
    NOMINMAX

    _CRT_SECURE_NO_WARNINGS
    _SCL_SECURE_NO_WARNINGS

    # Boost
    BOOST_ALL_DYN_LINK
    BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE

    # Needed to prevent Python from adding a define for snprintf
    # since it was added in Visual Studio 2015.
    HAVE_SNPRINTF
)

#------------------------------------------------------------------------------
# compiler configuration
#------------------------------------------------------------------------------
# Do not use GNU extension 
# Use -std=c++11 instead of -std=gnu++11
set(CMAKE_CXX_EXTENSIONS OFF)

function(mayaHydra_compile_config TARGET)
    # required compiler feature
    # Require C++14 if we're either building for Maya 2019 or later, or if we're building against 
    # USD 20.05 or later. Otherwise require C++11.
    if ((MAYA_APP_VERSION VERSION_GREATER_EQUAL 2019) OR (PXR_VERSION VERSION_GREATER_EQUAL 2005))
        target_compile_features(${TARGET} 
            PRIVATE
                # USD updated to c++17 for USD v23.11
                $<IF:$<VERSION_GREATER_EQUAL:${USD_VERSION},0.23.11>,cxx_std_17,cxx_std_14>
        )
    else()
        target_compile_features(${TARGET} 
            PRIVATE
                cxx_std_11
        )
    endif()
    if(IS_GNU OR IS_CLANG)
        target_compile_options(${TARGET} 
            PRIVATE
                ${GNU_CLANG_FLAGS}
        )
        if(IS_LINUX)
            target_compile_definitions(${TARGET}
                PRIVATE
                    _GLIBCXX_USE_CXX11_ABI=$<IF:$<BOOL:${MAYA_LINUX_BUILT_WITH_CXX11_ABI}>,1,0>
            )
        endif()
        if(USD_VERSION VERSION_GREATER_EQUAL "0.23.11")
            # Parts of boost rely on deprecated features of STL that have been removed
            # from some implementations under C++17 (which USD updated to for 23.11).
            # This define tells boost not to use those features.
            # With Visual Studio, boost automatically detects that this flag is needed.
            target_compile_definitions(${TARGET}
                PRIVATE
                    BOOST_NO_CXX98_FUNCTION_BASE
            )
        endif()
    elseif(IS_MSVC)
        target_compile_options(${TARGET} 
            PRIVATE
                ${MSVC_FLAGS}
        )
        target_compile_definitions(${TARGET} 
            PRIVATE
                ${MSVC_DEFINITIONS}
                # In debug builds, Boost's wrap_python.hpp will #undef _DEBUG when BOOST_DEBUG_PYTHON is not defined,
                # and will then include pyconfig.h :
                # https://github.com/boostorg/python/blob/boost-1.81.0/include/boost/python/detail/wrap_python.hpp#L23-L57
                # pyconfig.h will then autolink the Python lib; however, since _DEBUG was #undef'd, it will try to
                # link against the release version of the Python lib, causing a linker error :
                # https://github.com/python/cpython/blob/v3.11.4/PC/pyconfig.h#L269-L286
                $<$<STREQUAL:${CMAKE_BUILD_TYPE},Debug>:BOOST_DEBUG_PYTHON>
        )
    endif()

    # Common definitions
    target_compile_definitions(${TARGET}
        PRIVATE
            TBB_SUPPRESS_DEPRECATED_MESSAGES # Remove TBB deprecation warnings
            BOOST_ALL_NO_LIB # Avoid Boost autolinking libraries
    )
endfunction()
