# -----------------------------------------------------------------------------
# set target name
# -----------------------------------------------------------------------------
set(TARGET_NAME adskHydraSceneBrowser)

# -----------------------------------------------------------------------------
# Qt safeguard
# -----------------------------------------------------------------------------
if (NOT Qt6_FOUND)
    message(WARNING "No Qt6 package found. Cannot build ${TARGET_NAME}.")
    return()
endif()

# -----------------------------------------------------------------------------
# add library
# -----------------------------------------------------------------------------
add_library(${TARGET_NAME} SHARED adskHydraSceneBrowserApi.h)

# -----------------------------------------------------------------------------
# setup sources
# -----------------------------------------------------------------------------
set(HEADERS
    dataSourceTreeWidget.h
    dataSourceValueTreeView.h
    registeredSceneIndexChooser.h
    sceneIndexDebuggerWidget.h
    sceneIndexObserverLoggingTreeView.h
    sceneIndexObserverLoggingWidget.h
    sceneIndexTreeWidget.h
)

if(PXR_VERSION VERSION_GREATER_EQUAL 2408)
    list(APPEND HEADERS api.h)
endif()

set(SOURCES
    dataSourceTreeWidget.cpp
    dataSourceValueTreeView.cpp
    registeredSceneIndexChooser.cpp
    sceneIndexDebuggerWidget.cpp
    sceneIndexObserverLoggingTreeView.cpp
    sceneIndexObserverLoggingWidget.cpp
    sceneIndexTreeWidget.cpp
)

# Get the versioning scheme used in the file download URLs : last two digits of the year + digits of the month
# We first strip the major version (e.g. 0.23.8 -> 23.8)
string(REGEX REPLACE "^[0-9]+\\.([0-9]+\\.[0-9]+)$" "\\1" USD_minor_patch_version ${USD_VERSION})
# We then re-insert leading 0s to single-digit numbers (e.g. 23.8 -> 23.08)
# (note that \\10 works because CMake only captures up to 9 subgroups)
string(REGEX REPLACE "(\\.|^)([1-9])(\\.|$)" "\\10\\2\\3" USD_minor_patch_version ${USD_minor_patch_version})
set(DOWNLOAD_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/src/download")
set(PATCH_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/src/patch")

# Download a single hdui source/header file from OpenUSD, failing the configure
# stage loudly if the transfer does not succeed. The upstream file(DOWNLOAD) call
# used to run without any error handling, so a transient network failure or a
# truncated response silently produced an empty file. That empty translation unit
# then compiled cleanly but dropped every symbol it should have defined, surfacing
# much later as a confusing link error (undefined vtable/symbols) instead of a
# clear configure-time failure.
macro(download file)
    set(_download_url "https://raw.githubusercontent.com/PixarAnimationStudios/OpenUSD/v${USD_minor_patch_version}/extras/imaging/examples/hdui/${file}")
    set(_download_dest "${DOWNLOAD_LOCATION}/${file}")
    set(_download_max_attempts 3)
    set(_download_succeeded FALSE)

    foreach(_download_attempt RANGE 1 ${_download_max_attempts})
        if(EXISTS "${_download_dest}")
            file(REMOVE "${_download_dest}")
        endif()

        file(DOWNLOAD
            "${_download_url}"
            "${_download_dest}"
            STATUS _download_status
            TLS_VERIFY ON)

        list(GET _download_status 0 _download_code)
        list(GET _download_status 1 _download_message)
        if(_download_code EQUAL 0)
            # A zero status only means the transfer completed; guard against an empty body
            # (e.g. a proxy/CDN returning a 0-byte 200) as well.
            file(SIZE "${_download_dest}" _download_size)
            if(_download_size GREATER 0)
                set(_download_succeeded TRUE)
                break()
            endif()
            set(_download_code 1)
            set(_download_message "Downloaded file is empty")
        endif()

        if(_download_attempt LESS _download_max_attempts)
            message(STATUS
                "Download attempt ${_download_attempt}/${_download_max_attempts} for '${file}' failed "
                "(${_download_code} - ${_download_message}); retrying...")
        endif()
    endforeach()

    if(NOT _download_succeeded)
        if(EXISTS "${_download_dest}")
            file(REMOVE "${_download_dest}")
        endif()
        message(FATAL_ERROR
            "Failed to download '${file}' for ${TARGET_NAME} after ${_download_max_attempts} attempts.\n"
            "  URL   : ${_download_url}\n"
            "  Error : ${_download_code} - ${_download_message}")
    endif()
endmacro()

# Insert an include directive at the top a file.
macro(prepend_include FILE_CONTENTS INCLUDE_DIRECTIVE)
    set(${FILE_CONTENTS} "${INCLUDE_DIRECTIVE}\n${${FILE_CONTENTS}}")
endmacro()

# Regex to match a valid variable or class name
set(IDENTIFIER_REGEX [a-zA-Z_][a-zA-Z_0-9]*)

# Download and patch source files
foreach(SOURCE IN ITEMS ${SOURCES})
    download(${SOURCE})

    file(READ "${DOWNLOAD_LOCATION}/${SOURCE}" FILE_CONTENTS)

    # We can't construct a QVariant from a C string in Qt6. What we can do however is construct a QVariant
    # from a QLatin1StringView constructed with a C string. To patch this in we do the following replacements:
    # "QVariant(someVariable.str().c_str())" -> "QVariant(QLatin1StringView(someVariable.str().c_str()))"
    # "QVariant(someVariable.str().data())" -> "QVariant(QLatin1StringView(someVariable.str().data()))"
	if(PXR_VERSION VERSION_LESS 2408)
		string(REGEX REPLACE "QVariant\\(\(${IDENTIFIER_REGEX}\)\.str\\(\\)\.\(c_str|data\)\\(\\)\\)" "QVariant(QLatin1StringView(\\1.str().\\2()))" FILE_CONTENTS "${FILE_CONTENTS}")
	endif()
    
    # Some setExpanded() calls are wrapped inside a QTimer::singleShot() call in order to defer their execution.
    # This was done by Pixar in order to work around a crash in what appears to be some of their proprietary code
    # (a method named PhdRequest::ExtractOptionalValue). However, this workaround now causes a crash on our end.
    # Since the original intent was to directly call setExpanded(), we replace the QTimer::singleShot() workaround 
    # with a direct call to setExpanded(). This string regex does the following replacement :
    # QTimer::singleShot(0, [this]() {
    #     this->setExpanded(true);          ->      setExpanded(true);
    # });
    set(SET_EXPANDED_REGEX "QTimer::singleShot\\(0, \\[this\\]\\(\\) {[\r\n\t ]+this->setExpanded\\(true\\);[\r\n\t ]+}\\);")
    set(SET_EXPANDED_REPLACEMENT "setExpanded\(true\);")
    string(REGEX REPLACE "${SET_EXPANDED_REGEX}" "${SET_EXPANDED_REPLACEMENT}" FILE_CONTENTS "${FILE_CONTENTS}")

    # Patch in support for VtArray<bool>.  Arbitrarily place it after support
    # for VtArray<GfVec2f>.  For simplicity we run this on each file, even
    # though it is relevant only for dataSourceValueTreeView.cpp
    set(VTARRAY_BOOL_TARGET_REGEX "return new Hdui_TypedArrayValueItemModel<GfVec2f>\\(value, parent\\);[\r\n\t ]+}")
    set(VTARRAY_BOOL_TARGET_REPLACEMENT "return new Hdui_TypedArrayValueItemModel<GfVec2f>(value, parent);\n    }\n\n    if (value.IsHolding<VtArray<bool>>()) {\n        return new Hdui_TypedArrayValueItemModel<bool>(value, parent);\n    }")
    string(REGEX REPLACE "${VTARRAY_BOOL_TARGET_REGEX}" "${VTARRAY_BOOL_TARGET_REPLACEMENT}" FILE_CONTENTS "${FILE_CONTENTS}")

    # Patch in support for VtArray<GfVec4f>.  Arbitrarily place it after support
    # for VtArray<GfVec3f>.  For simplicity we run this on each file, even
    # though it is relevant only for dataSourceValueTreeView.cpp
    set(VTARRAY_GFVEC4F_TARGET_REGEX "return new Hdui_TypedArrayValueItemModel<GfVec3f>\\(value, parent\\);[\r\n\t ]+}")
    set(VTARRAY_GFVEC4F_TARGET_REPLACEMENT "return new Hdui_TypedArrayValueItemModel<GfVec3f>(value, parent);\n    }\n\n    if (value.IsHolding<VtArray<GfVec4f>>()) {\n        return new Hdui_TypedArrayValueItemModel<GfVec4f>(value, parent);\n    }")
    string(REGEX REPLACE "${VTARRAY_GFVEC4F_TARGET_REGEX}" "${VTARRAY_GFVEC4F_TARGET_REPLACEMENT}" FILE_CONTENTS "${FILE_CONTENTS}")

    # Patch in '#include <QString>' to make sure we can use QLatin1StringView
    prepend_include(FILE_CONTENTS "#include <QString>")

    # Remove an unused lambda capture to fix a warning-treated-as-error on MacOS
	if(PXR_VERSION VERSION_LESS 2408)
		string(REGEX REPLACE "\(\\[this, menu\), menuTreeWidget\(\\]\\(QTreeWidgetItem \\*item, int column\\)\)" "\\1\\2" FILE_CONTENTS "${FILE_CONTENTS}")
	endif()
    
    file(WRITE "${PATCH_LOCATION}/${SOURCE}" "${FILE_CONTENTS}")
endforeach()

# Download and patch header files
foreach(HEADER IN ITEMS ${HEADERS})
    download(${HEADER})

    file(READ "${DOWNLOAD_LOCATION}/${HEADER}" FILE_CONTENTS)

    # Add DLL import/export support for Windows 
    # Insert 'HDUI_API' symbols for class declarations
    # Note: we assume class declarations are always followed by double colons
	if(PXR_VERSION VERSION_LESS 2408)
		string(REGEX REPLACE "class (${IDENTIFIER_REGEX}[^\;]:)" "class HDUI_API \\1" FILE_CONTENTS "${FILE_CONTENTS}")
	endif()
    
	if(PXR_VERSION VERSION_LESS 2408)
		# Patch in '#include <adskHydraSceneBrowserApi.h>' to have HDUI_API defined
		prepend_include(FILE_CONTENTS "#include <adskHydraSceneBrowserApi.h>")
    endif()
	
    file(WRITE "${PATCH_LOCATION}/${HEADER}" "${FILE_CONTENTS}")
endforeach()

# Prepend an absolute path to the downloaded source files, located in 
# the CMAKE_CURRENT_BINARY_DIR.
# Note: the sources are downloaded for PixarAnimationStudios/OpenUSD and 
#       are not located in this repository's folder
list(TRANSFORM SOURCES PREPEND "${PATCH_LOCATION}/")

# Add CMAKE_CURRENT_BINARY_DIR downloaded sources to the adskHydraSceneBrowser target
target_sources(${TARGET_NAME}
    PRIVATE
        ${SOURCES}
)

# -----------------------------------------------------------------------------
# include directories
# -----------------------------------------------------------------------------
target_include_directories(${TARGET_NAME} 
    PUBLIC
        ${PXR_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PATCH_LOCATION}
)

# -----------------------------------------------------------------------------
# compiler configuration
# -----------------------------------------------------------------------------
# QT_NO_KEYWORDS prevents Qt from defining the foreach, signals, slots and emit macros.
# this avoids overlap between Qt macros and boost, and enforces using Q_ macros.
get_target_property(qt_core_aliased Qt6::Core ALIASED_TARGET)
if (qt_core_aliased)
    set_target_properties(${qt_core_aliased} PROPERTIES INTERFACE_COMPILE_DEFINITIONS QT_NO_KEYWORDS)
else()
    set_target_properties(Qt6::Core PROPERTIES INTERFACE_COMPILE_DEFINITIONS QT_NO_KEYWORDS)
endif()

target_compile_definitions(${TARGET_NAME} 
	PRIVATE 
        $<IF:$<VERSION_LESS:${PXR_VERSION},2408>,HDUI_EXPORT,HDUI_EXPORTS>
)

mayaHydra_compile_config(${TARGET_NAME}) # TODO : This will have to be changed when moving to a standalone repo.

# -----------------------------------------------------------------------------
# link libraries
# -----------------------------------------------------------------------------
target_link_libraries(${TARGET_NAME} 
    PUBLIC
        usd
        usdImaging
        Qt6::Core
        Qt6::Widgets
)

# -----------------------------------------------------------------------------
# Qt file processing (moc)
# -----------------------------------------------------------------------------
set_property(TARGET adskHydraSceneBrowser PROPERTY AUTOMOC ON)

# -----------------------------------------------------------------------------
# install
# -----------------------------------------------------------------------------
install(TARGETS ${TARGET_NAME}
        LIBRARY
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

list(TRANSFORM HEADERS PREPEND "${PATCH_LOCATION}/")
install(FILES ${HEADERS}
        DESTINATION 
            ${CMAKE_INSTALL_PREFIX}/include/adskHydraSceneBrowser
)

if(IS_WINDOWS)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> 
            DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)
endif()
