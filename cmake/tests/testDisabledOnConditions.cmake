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

# Unit test for the disable tag semantics in cmake/testConditions.cmake: the
# disabledOn: conditions, covering exact and ranged Maya version predicates,
# platform and renderer facts, the conjunction of several conditions, and
# rejection of conditions that could never hold; the tracking issues a
# disabled: group must name; and the shape of both groups.

# Script mode starts with all policies OLD, which leaves the IN_LIST operator
# the module relies on unsupported. Match the project instead.
cmake_minimum_required(VERSION 3.21)

set(_conditions_module "${CMAKE_CURRENT_LIST_DIR}/../testConditions.cmake")
include("${_conditions_module}")

# Conditions are spelled comma separated here, as in a disabledOn: tag.
function(expect_disabled_on condition_list expected)
    string(REPLACE "," ";" _conditions "${condition_list}")
    mayaHydra_disabled_on_group_matches(_matches "${_conditions}")
    if(NOT _matches STREQUAL expected)
        message(FATAL_ERROR
            "disabledOn:${condition_list} with Maya ${MAYA_APP_VERSION} on "
            "${IS_MACOSX}/${IS_WINDOWS}/${IS_LINUX} under "
            "${MAYAHYDRA_TEST_RENDERER} expected ${expected}, got ${_matches}")
    endif()
endfunction()

# The rejection paths end in message(FATAL_ERROR), so they need a child process.
# call_line is the module call to attempt, and marker must appear in whatever
# diagnostic it produces.
function(expect_rejected call_line marker description)
    set(_script "${CMAKE_CURRENT_BINARY_DIR}/disableTagRejection.cmake")
    file(WRITE "${_script}"
        "cmake_minimum_required(VERSION 3.21)\n"
        "include(\"${_conditions_module}\")\n"
        "set(MAYA_APP_VERSION 2027)\n"
        "set(IS_WINDOWS TRUE)\n"
        "set(MAYAHYDRA_TEST_RENDERER HdStorm)\n"
        "${call_line}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -P "${_script}"
        RESULT_VARIABLE _result
        OUTPUT_QUIET
        ERROR_VARIABLE _stderr
    )
    file(REMOVE "${_script}")
    if(_result EQUAL 0)
        message(FATAL_ERROR
            "Expected ${description} to be rejected, but it was accepted")
    endif()
    # Require our own diagnostic, so that an unrelated failure in the child
    # cannot pass this off as a rejection.
    if(NOT _stderr MATCHES "${marker}")
        message(FATAL_ERROR
            "${description} failed for an unexpected reason:\n${_stderr}")
    endif()
endfunction()

function(expect_disabled_on_rejected condition_list)
    string(REPLACE "," ";" _conditions "${condition_list}")
    expect_rejected(
        "mayaHydra_disabled_on_group_matches(_matches \"${_conditions}\")"
        "disabledOn:"
        "disabledOn:${condition_list}")
endfunction()

# Issues are spelled comma separated here, as in a disabled: tag. An accepted
# list is one that does not abort, so these need no child process.
function(expect_tracking_issues_accepted issue_list)
    string(REPLACE "," ";" _issues "${issue_list}")
    mayaHydra_validate_tracking_issues("${_issues}" "disabled:")
endfunction()

function(expect_tracking_issues_rejected issue_list)
    string(REPLACE "," ";" _issues "${issue_list}")
    expect_rejected(
        "mayaHydra_validate_tracking_issues(\"${_issues}\" \"disabled:\")"
        "disabled:"
        "disabled:${issue_list}")
endfunction()

# A whole disabledOn: group, with the two halves spelled as the tag parser
# hands them over. Checks the issues come back as well as the match result,
# since the caller turns them into labels.
function(expect_disabled_on_group conditions issues expected_matches)
    mayaHydra_parse_disabled_on_group(_matches _issues "${conditions};${issues}")
    if(NOT _matches STREQUAL expected_matches)
        message(FATAL_ERROR
            "disabledOn:${conditions}:${issues} with Maya ${MAYA_APP_VERSION} on "
            "${IS_MACOSX}/${IS_WINDOWS}/${IS_LINUX} under "
            "${MAYAHYDRA_TEST_RENDERER} expected ${expected_matches}, got ${_matches}")
    endif()
    string(REPLACE "," ";" _expected_issues "${issues}")
    if(NOT "${_issues}" STREQUAL "${_expected_issues}")
        message(FATAL_ERROR
            "disabledOn:${conditions}:${issues} returned issues ${_issues}")
    endif()
endfunction()

function(expect_disabled_on_group_rejected conditions issues)
    expect_rejected(
        "mayaHydra_parse_disabled_on_group(_m _i \"${conditions};${issues}\")"
        "disabledOn:"
        "disabledOn:${conditions}:${issues}")
endfunction()

# A whole disabled: group, with its values spelled as the tag parser hands
# them over: already split on the colon, issues still comma separated.
function(expect_disabled_group issues)
    mayaHydra_parse_disabled_group(_issues "${issues}")
    string(REPLACE "," ";" _expected_issues "${issues}")
    if(NOT "${_issues}" STREQUAL "${_expected_issues}")
        message(FATAL_ERROR "disabled:${issues} returned issues ${_issues}")
    endif()
endfunction()

function(expect_disabled_group_rejected group_values description)
    expect_rejected(
        "mayaHydra_parse_disabled_group(_i \"${group_values}\")"
        "disabled:"
        "${description}")
endfunction()

set(MAYA_APP_VERSION 2027)
set(IS_WINDOWS TRUE)
set(MAYAHYDRA_TEST_RENDERER HdStorm)

expect_disabled_on(maya2027 TRUE)
expect_disabled_on(maya2026 FALSE)

expect_disabled_on(mayaGe2026 TRUE)
expect_disabled_on(mayaGe2027 TRUE)
expect_disabled_on(mayaGt2026 TRUE)
expect_disabled_on(mayaGt2027 FALSE)
expect_disabled_on(mayaLe2027 TRUE)
expect_disabled_on(mayaLe2026 FALSE)
expect_disabled_on(mayaLt2028 TRUE)
expect_disabled_on(mayaLt2027 FALSE)

expect_disabled_on(win TRUE)
expect_disabled_on(osx FALSE)
expect_disabled_on(HdStorm TRUE)
expect_disabled_on(HdFlash FALSE)

# A group holds only when every one of its conditions does.
expect_disabled_on(maya2027,win TRUE)
expect_disabled_on(maya2027,osx FALSE)
expect_disabled_on(maya2026,win FALSE)
expect_disabled_on(mayaGe2026,win,HdStorm TRUE)
expect_disabled_on(mayaGe2026,win,HdFlash FALSE)

# A group also names the issues tracking the work of re-enabling the test,
# which the caller turns into labels only when the group matches.
expect_disabled_on_group(maya2027,win HYDRA-1940 TRUE)
expect_disabled_on_group(maya2027,osx HYDRA-1940 FALSE)
expect_disabled_on_group(maya2027,win AGP-1034,AGP-1035 TRUE)

set(MAYA_APP_VERSION 2026)
expect_disabled_on(maya2026 TRUE)
expect_disabled_on(maya2027 FALSE)
expect_disabled_on(mayaGe2026 TRUE)
expect_disabled_on(mayaGe2027 FALSE)
expect_disabled_on(mayaGt2026 FALSE)
expect_disabled_on(mayaLe2026 TRUE)
expect_disabled_on(mayaLt2027 TRUE)

# A condition may name a Maya version that does not exist yet: it stays false
# until someone configures against that version, with no change to this file.
set(MAYA_APP_VERSION 2028)
expect_disabled_on(mayaGt2027 TRUE)
expect_disabled_on(mayaGe2027 TRUE)
expect_disabled_on(maya2027 FALSE)

set(MAYA_APP_VERSION 2027)
set(IS_WINDOWS FALSE)
set(IS_MACOSX TRUE)
expect_disabled_on(osx TRUE)
expect_disabled_on(win FALSE)
expect_disabled_on(maya2027,osx TRUE)

set(MAYAHYDRA_TEST_RENDERER HdFlash)
expect_disabled_on(HdFlash TRUE)
expect_disabled_on(HdStorm FALSE)

# A condition that can never hold would turn its whole group into a no-op, so
# anything unrecognized is an error rather than a silent false.
expect_disabled_on_rejected(mayaGE2026)  # Ge/Gt/Le/Lt are case sensitive
expect_disabled_on_rejected(maya26)
expect_disabled_on_rejected(mayaGe20267)
expect_disabled_on_rejected(macos)
expect_disabled_on_rejected(HdArnold)
expect_disabled_on_rejected(mtoa)        # plugins belong to depOnPlugins:
expect_disabled_on_rejected(maya2027,osxx)

# An empty group would vacuously hold and disable the test everywhere.
expect_disabled_on_rejected("")

# Evaluating renderer facts before the renderer is defined must not silently
# make every renderer condition false.
expect_rejected(
    "unset(MAYAHYDRA_TEST_RENDERER)\nmayaHydra_disabled_on_group_matches(_m \"win\")"
    "MAYAHYDRA_TEST_RENDERER is not set"
    "disabledOn: evaluated with no test renderer")

# A disabled: group must name the work of re-enabling the test.
expect_tracking_issues_accepted(HYDRA-1940)
expect_tracking_issues_accepted(AGP-1034)
expect_tracking_issues_accepted(OGSMOD-5546)
expect_tracking_issues_accepted(AGP-1034,AGP-1035)

expect_tracking_issues_rejected(hydra-1940)  # project keys are upper case
expect_tracking_issues_rejected(HYDRA)
expect_tracking_issues_rejected(1940)
expect_tracking_issues_rejected(HYDRA-)
expect_tracking_issues_rejected(HYDRA-1940x)
expect_tracking_issues_rejected(HYDRA-1940,oops)
expect_tracking_issues_rejected("")

# A disabledOn: group must name one too. The child runs on Windows, so the
# first group does not match there: a malformed issue has to be an error on
# every configuration, or a typo would hide until someone builds on the
# platform the group names.
expect_disabled_on_group_rejected(maya2027,osx oops)
expect_disabled_on_group_rejected(maya2027,win "")

# Neither half may be left out, and there is no third half.
expect_rejected(
    "mayaHydra_parse_disabled_on_group(_m _i \"maya2027,win\")"
    "disabledOn:"
    "disabledOn:maya2027,win with no issues")
expect_rejected(
    "mayaHydra_parse_disabled_on_group(_m _i \"maya2027,win;HYDRA-1940;extra\")"
    "disabledOn:"
    "disabledOn:maya2027,win:HYDRA-1940:extra")

# A disabled: group has a single value. Several issues are comma separated; a
# colon in their place must not silently drop the issues after it.
expect_disabled_group(HYDRA-1940)
expect_disabled_group(AGP-1034,AGP-1035)

expect_disabled_group_rejected("HYDRA-1;HYDRA-2" "disabled:HYDRA-1:HYDRA-2")
expect_disabled_group_rejected("HYDRA-1;" "disabled:HYDRA-1: with a trailing colon")
expect_disabled_group_rejected(oops "disabled:oops")
expect_disabled_group_rejected("" "disabled: with no issues")
