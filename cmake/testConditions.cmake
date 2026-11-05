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
# issues.
#
# Keeping this logic separate also allows testing it in CMake script mode.

# Renderers the tests can run against. Single source of truth: the test suite
# selects one of these, and they double as disabledOn: conditions.
set(MAYAHYDRA_TEST_RENDERER_CHOICES HdStorm HdFlash)

# Platforms usable as disabledOn: conditions. Same spellings as the older
# skipOnPlatform: tag and as platforms_to_exclude in tests-to-run.json.
set(MAYAHYDRA_TEST_PLATFORM_CHOICES osx win lin)

# The platform and renderer this build configures its tests for. Maya versions
# are deliberately absent: they are predicates rather than facts, see
# mayaHydra_test_condition_holds.
function(mayaHydra_current_test_facts out)
    if(IS_MACOSX)
        set(platform osx)
    elseif(IS_WINDOWS)
        set(platform win)
    else()
        set(platform lin)
    endif()
    set(${out} ${platform} ${MAYAHYDRA_TEST_RENDERER} PARENT_SCOPE)
endfunction()

# Evaluates one disabledOn: condition.
#
# Maya version conditions are maya<op><year>, with op one of Ge/Gt/Le/Lt or
# absent for an exact match. The year is compared against MAYA_APP_VERSION and
# never looked up in a table of known releases, so a condition may name a Maya
# version that does not exist yet: mayaGt2027 simply holds the first time
# someone configures against a later version.
#
# Every other condition names a platform or a renderer, and holds when it is
# what this build configures its tests for.
function(mayaHydra_test_condition_holds out condition)
    if(condition MATCHES "^maya(Ge|Gt|Le|Lt)?([0-9][0-9][0-9][0-9])$")
        set(operator "${CMAKE_MATCH_1}")
        set(year "${CMAKE_MATCH_2}")
        if(operator STREQUAL "")
            set(comparison VERSION_EQUAL)
        elseif(operator STREQUAL "Ge")
            set(comparison VERSION_GREATER_EQUAL)
        elseif(operator STREQUAL "Gt")
            set(comparison VERSION_GREATER)
        elseif(operator STREQUAL "Le")
            set(comparison VERSION_LESS_EQUAL)
        else()
            set(comparison VERSION_LESS)
        endif()
        if(MAYA_APP_VERSION ${comparison} ${year})
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
            "Unknown disabledOn: condition \"${condition}\". Expected maya<year>, "
            "optionally prefixed with Ge/Gt/Le/Lt, or one of "
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
