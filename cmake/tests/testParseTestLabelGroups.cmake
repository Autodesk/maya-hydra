#
# Copyright 2026 Autodesk
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Unit test for mayaHydra_parse_test_label_groups in cmake/testLabels.cmake:
# splitting a tag into groups, rejecting bare disable keys, the sentinel and
# issue labels the disable groups yield, handing every other group to
# find_labels, and accepting a tag both as a raw string and as a split list.
# The disable group semantics themselves are covered by
# testDisabledOnConditions.cmake.

# Script mode starts with all policies OLD, which leaves the IN_LIST operator
# the module relies on unsupported. Match the project instead.
cmake_minimum_required(VERSION 3.21)

set(_labels_module "${CMAKE_CURRENT_LIST_DIR}/../testLabels.cmake")
include("${_labels_module}")

# Compared as strings, so that the order and any duplicate are checked too.
function(expect_labels tag_groups expected)
    mayaHydra_parse_test_label_groups(_labels "${tag_groups}")
    if(NOT "${_labels}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "Tag \"${tag_groups}\" expected labels \"${expected}\", got \"${_labels}\"")
    endif()
endfunction()

function(expect_no_label tag_groups label)
    mayaHydra_parse_test_label_groups(_labels "${tag_groups}")
    if(label IN_LIST _labels)
        message(FATAL_ERROR
            "Tag \"${tag_groups}\" must not yield label \"${label}\", got \"${_labels}\"")
    endif()
endfunction()

# The rejection paths end in message(FATAL_ERROR), so they need a child process.
# marker must appear in whatever diagnostic the rejected tag produces.
function(expect_rejected tag_groups marker)
    set(_script "${CMAKE_CURRENT_BINARY_DIR}/parseTestLabelGroupsRejection.cmake")
    file(WRITE "${_script}"
        "cmake_minimum_required(VERSION 3.21)\n"
        "include(\"${_labels_module}\")\n"
        "set(MAYA_APP_VERSION 2027)\n"
        "set(IS_WINDOWS TRUE)\n"
        "set(MAYAHYDRA_TEST_RENDERER HdStorm)\n"
        "mayaHydra_parse_test_label_groups(_labels \"${tag_groups}\")\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -P "${_script}"
        RESULT_VARIABLE _result
        OUTPUT_QUIET
        ERROR_VARIABLE _stderr
    )
    file(REMOVE "${_script}")
    if(_result EQUAL 0)
        message(FATAL_ERROR "Expected tag \"${tag_groups}\" to be rejected, but it was accepted")
    endif()
    # Require our own diagnostic, so that an unrelated failure in the child
    # cannot pass this off as a rejection.
    if(NOT _stderr MATCHES "${marker}")
        message(FATAL_ERROR
            "Tag \"${tag_groups}\" failed for an unexpected reason:\n${_stderr}")
    endif()
endfunction()

set(MAYA_APP_VERSION 2027)
set(IS_WINDOWS TRUE)
set(MAYAHYDRA_TEST_RENDERER HdStorm)

# A tag splits into groups on "|", and a group's values on ",".
expect_labels("depOnPlugins:mtoa" "mtoa")
expect_labels("depOnPlugins:mtoa|skipOnPlatform:osx" "mtoa;osx")
expect_labels("depOnPlugins:mtoa,lookdevx" "mtoa;lookdevx")

# A disable key with nothing after it is rejected by the parser itself, before
# the group reaches testConditions.cmake. With a trailing colon it gets there,
# and is rejected there instead.
expect_rejected("disabledOn" "must be disabledOn:")
expect_rejected("disabled" "has no tracking issue")
expect_rejected("depOnPlugins:mtoa|disabledOn" "must be disabledOn:")
expect_rejected("disabledOn:" "disabledOn:")
expect_rejected("disabled:" "disabled:")

# A matching disabledOn: group yields its sentinel and issues, a non-matching
# one yields nothing, and a disabled: group always yields its sentinel and issues.
expect_labels("disabledOn:maya2027,win:HYDRA-1940" "disabledOnCurrentConfig;HYDRA-1940")
expect_labels("disabledOn:maya2027,osx:HYDRA-1940" "")
expect_labels("disabled:AGP-1034,AGP-1035" "disabled;AGP-1034;AGP-1035")

# Conditions decide whether a group disables, and never become labels.
foreach(_condition maya2027 win HdStorm)
    expect_no_label("disabledOn:maya2027,win,HdStorm:HYDRA-1940" ${_condition})
endforeach()

# Every other group goes to find_labels, and a group yielding nothing must not
# carry over the labels of the group before it.
expect_labels("depOnPlugins:mtoa|disabledOn:maya2027,osx:HYDRA-1|skipOnPlatform:lin"
    "mtoa;lin")
expect_labels("depOnPlugins:mtoa|disabledOn:maya2027,osx:HYDRA-1" "mtoa")

# Groups of every kind combine, in tag order.
expect_labels(
    "depOnPlugins:mtoa|disabledOn:maya2027,win:HYDRA-1940|disabled:AGP-1034"
    "mtoa;disabledOnCurrentConfig;HYDRA-1940;disabled;AGP-1034")
expect_labels(
    "disabled:AGP-1034|depOnPlugins:mtoa|disabledOn:mayaGe2026:HYDRA-1940"
    "disabled;AGP-1034;mtoa;disabledOnCurrentConfig;HYDRA-1940")

# cmdLineRender/CMakeLists.txt passes the raw tag, while get_testfile_and_labels
# passes it already split on "|". Both must give the same labels.
set(_raw "depOnPlugins:mtoa|disabledOn:maya2027,win:HYDRA-1940|disabled:AGP-1034")
string(REPLACE "|" ";" _split "${_raw}")
mayaHydra_parse_test_label_groups(_from_raw "${_raw}")
mayaHydra_parse_test_label_groups(_from_split "${_split}")
if(NOT "${_from_raw}" STREQUAL "${_from_split}")
    message(FATAL_ERROR
        "Raw tag gave \"${_from_raw}\" but the split tag gave \"${_from_split}\"")
endif()
expect_labels("${_split}" "mtoa;disabledOnCurrentConfig;HYDRA-1940;disabled;AGP-1034")

# A test with no tag at all gets no label.
expect_labels("" "")
