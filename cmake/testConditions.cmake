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

# Configure-time semantics of the two disable groups of a test tag.
#
# A disabledOn: group lists conditions that must all hold for the test to be
# disabled, and the issues tracking the work of re-enabling it. The label-based
# tags cannot express the conjunction: CTest matches labels independently, so
# several label-based skips on one test are ORed, never ANDed. Conditions are
# therefore evaluated here, at configure time, and never become labels
# themselves. The issues do, but only where the group actually disables.
#
# A disabled: group disables the test everywhere and names the same kind of
# issues. It has a single value, the comma separated issues.
#
# Keeping this logic separate also allows testing it in CMake script mode.

# Renderers the tests can run against. Single source of truth: the test suite
# selects one of these, and they double as disabledOn: conditions.
set(MAYAHYDRA_TEST_RENDERER_CHOICES HdStorm HdFlash)

# Platforms usable as disabledOn: conditions. Same spellings as the older
# skipOnPlatform: tag and as platforms_to_exclude in tests-to-run.json.
set(MAYAHYDRA_TEST_PLATFORM_CHOICES osx win lin)

# The platform and renderer this build configures its tests for. Maya and USD
# versions are deliberately absent: they are predicates rather than facts, see
# mayaHydra_test_condition_holds.
function(mayaHydra_current_test_facts out)
    if(NOT MAYAHYDRA_TEST_RENDERER)
        message(FATAL_ERROR
            "MAYAHYDRA_TEST_RENDERER is not set: disabledOn: renderer "
            "conditions cannot be evaluated before cmake/test.cmake defines it.")
    endif()
    if(IS_MACOSX)
        set(platform osx)
    elseif(IS_WINDOWS)
        set(platform win)
    else()
        set(platform lin)
    endif()
    set(${out} ${platform} ${MAYAHYDRA_TEST_RENDERER} PARENT_SCOPE)
endfunction()

# Maps the operator of a version condition, one of Ge/Gt/Le/Lt or empty for an
# exact match, to the if() version comparison it stands for.
function(_mayaHydra_version_comparison out operator)
    if(operator STREQUAL "")
        set(${out} VERSION_EQUAL PARENT_SCOPE)
    elseif(operator STREQUAL "Ge")
        set(${out} VERSION_GREATER_EQUAL PARENT_SCOPE)
    elseif(operator STREQUAL "Gt")
        set(${out} VERSION_GREATER PARENT_SCOPE)
    elseif(operator STREQUAL "Le")
        set(${out} VERSION_LESS_EQUAL PARENT_SCOPE)
    else()
        set(${out} VERSION_LESS PARENT_SCOPE)
    endif()
endfunction()

# Evaluates one disabledOn: condition.
#
# Maya version conditions are maya<op><year>, with op one of Ge/Gt/Le/Lt or
# absent for an exact match. The year is compared against MAYA_APP_VERSION and
# never looked up in a table of known releases, so a condition may name a Maya
# version that does not exist yet: mayaGt2027 simply holds the first time
# someone configures against a later version.
#
# USD version conditions are usd<op><version>, with the same operators. The
# version is spelled as PXR_VERSION encodes it, major * 10000 + minor * 100 +
# patch, so USD 0.26.11 is usd2611, and is compared against PXR_VERSION with
# the same freedom to name a release that does not exist yet.
#
# Every other condition names a platform or a renderer, and holds when it is
# what this build configures its tests for.
function(mayaHydra_test_condition_holds out condition)
    if(condition MATCHES "^maya(Ge|Gt|Le|Lt)?([0-9][0-9][0-9][0-9])$")
        set(year "${CMAKE_MATCH_2}")
        _mayaHydra_version_comparison(comparison "${CMAKE_MATCH_1}")
        if(MAYA_APP_VERSION ${comparison} ${year})
            set(${out} TRUE PARENT_SCOPE)
        else()
            set(${out} FALSE PARENT_SCOPE)
        endif()
        return()
    endif()

    if(condition MATCHES "^usd(Ge|Gt|Le|Lt)?([0-9][0-9][0-9][0-9])$")
        # Unset, every USD condition would silently be false.
        if(NOT DEFINED PXR_VERSION)
            message(FATAL_ERROR
                "PXR_VERSION is not set: disabledOn: USD conditions cannot be "
                "evaluated before USD is found.")
        endif()
        set(version "${CMAKE_MATCH_2}")
        _mayaHydra_version_comparison(comparison "${CMAKE_MATCH_1}")
        if(PXR_VERSION ${comparison} ${version})
            set(${out} TRUE PARENT_SCOPE)
        else()
            set(${out} FALSE PARENT_SCOPE)
        endif()
        return()
    endif()

    # Reject an unrecognized condition rather than letting it evaluate to false.
    # A condition that can never hold turns its whole group into a no-op, and
    # the test then runs everywhere with nothing reporting that the tag broke.
    if(NOT condition IN_LIST MAYAHYDRA_TEST_PLATFORM_CHOICES AND
       NOT condition IN_LIST MAYAHYDRA_TEST_RENDERER_CHOICES)
        message(FATAL_ERROR
            "Unknown disabledOn: condition \"${condition}\". Expected maya<year> "
            "or usd<version>, either optionally prefixed with Ge/Gt/Le/Lt, or one of "
            "${MAYAHYDRA_TEST_PLATFORM_CHOICES} "
            "${MAYAHYDRA_TEST_RENDERER_CHOICES}.")
    endif()

    mayaHydra_current_test_facts(facts)
    if(condition IN_LIST facts)
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

# A disabledOn: group matches when every one of its conditions holds.
function(mayaHydra_disabled_on_group_matches out conditions)
    list(LENGTH conditions condition_count)
    if(condition_count EQUAL 0)
        # An empty group would vacuously match and disable the test everywhere.
        message(FATAL_ERROR "A disabledOn: tag group must list at least one condition")
    endif()

    foreach(condition IN LISTS conditions)
        mayaHydra_test_condition_holds(holds "${condition}")
        if(NOT holds)
            set(${out} FALSE PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out} TRUE PARENT_SCOPE)
endfunction()

# Evaluates a disabledOn: group, whose values are "<conditions>:<issues>" and
# reach here already split on the colon.
#
# The issues are validated whatever the outcome, so that a malformed one is an
# error on every platform rather than only where the group happens to match.
function(mayaHydra_parse_disabled_on_group matches_out issues_out group_values)
    list(LENGTH group_values value_count)
    if(NOT value_count EQUAL 2)
        message(FATAL_ERROR
            "A disabledOn: tag group must be spelled disabledOn:<conditions>:<issues>, "
            "as in disabledOn:maya2027,osx:HYDRA-1940")
    endif()

    list(GET group_values 0 condition_values)
    list(GET group_values 1 issue_values)
    string(REPLACE "," ";" conditions "${condition_values}")
    string(REPLACE "," ";" issues "${issue_values}")

    mayaHydra_validate_tracking_issues("${issues}" "disabledOn:")
    mayaHydra_disabled_on_group_matches(matches "${conditions}")

    set(${matches_out} ${matches} PARENT_SCOPE)
    set(${issues_out} ${issues} PARENT_SCOPE)
endfunction()

# Evaluates a disabled: group, whose single value is "<issues>" and reaches
# here already split on the colon, so a stray colon shows up as a second value.
# Rejecting it matters: otherwise the issues after the colon are silently lost.
function(mayaHydra_parse_disabled_group issues_out group_values)
    list(LENGTH group_values value_count)
    if(NOT value_count EQUAL 1)
        message(FATAL_ERROR
            "A disabled: tag group must be spelled disabled:<issues>, "
            "with several issues separated by commas, as in disabled:HYDRA-1,HYDRA-2")
    endif()

    string(REPLACE "," ";" issues "${group_values}")
    mayaHydra_validate_tracking_issues("${issues}" "disabled:")

    set(${issues_out} ${issues} PARENT_SCOPE)
endfunction()

# Checks the issues named by a disable group, with group naming which one for
# the diagnostics. Requiring an issue of the form PROJECT-1234 is what keeps a
# disabled test attached to the work of re-enabling it, rather than becoming a
# test nobody remembers turning off.
function(mayaHydra_validate_tracking_issues issues group)
    list(LENGTH issues issue_count)
    if(issue_count EQUAL 0)
        message(FATAL_ERROR "A ${group} tag group must name at least one tracking issue")
    endif()

    foreach(issue IN LISTS issues)
        if(NOT issue MATCHES "^[A-Z]+-[0-9]+$")
            message(FATAL_ERROR
                "Malformed ${group} tracking issue \"${issue}\". "
                "Expected a project key and a number, such as HYDRA-1940.")
        endif()
    endforeach()
endfunction()
