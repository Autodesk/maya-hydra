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

# Configure-time parsing of test tags into CTest labels.
#
# This module defines functions only, with no configure-time side effects, so
# that it can be tested in CMake script mode.

include("${CMAKE_CURRENT_LIST_DIR}/testConditions.cmake")

function(find_labels label_set label_list)
    string(REPLACE ":" ";" split_labels ${label_set})
    list(LENGTH split_labels len)
    if(len GREATER 0)
        list(GET split_labels 1 labels_value)
        # we expect comma separated labels
        string(REPLACE "," ";" all_labels ${labels_value})
        set(local_label_list "")
        foreach(label ${all_labels})
            list(APPEND local_label_list ${label})
        endforeach()
        set(${label_list} ${local_label_list} PARENT_SCOPE)
    endif()
endfunction()

# Turns "|"-separated key:value tag groups into a label list.
#
# The two disable groups are evaluated here rather than contributing their
# values as labels. "disabledOn:" yields the disabledOnCurrentConfig sentinel
# plus a label per tracking issue, but only when its conditions match this
# configuration; its conditions never become labels. "disabled:" yields the
# disabled sentinel plus a label per tracking issue. See
# cmake/testConditions.cmake for what each accepts.
function(mayaHydra_parse_test_label_groups all_labels tag_groups)
    set(collect_labels "")
    # Accepts a raw tag string as well as an already-split list.
    string(REPLACE "|" ";" groups "${tag_groups}")
    foreach(item ${groups})
        string(REPLACE ":" ";" key_and_values "${item}")
        list(GET key_and_values 0 key)
        if(key STREQUAL "disabledOn")
            list(LENGTH key_and_values key_and_values_length)
            if(key_and_values_length LESS 2)
                message(FATAL_ERROR
                    "Tag group \"${item}\" must be disabledOn:<conditions>:<issues>")
            endif()
            # list(SUBLIST) rejects a begin index past the end, so the length
            # is checked above rather than left to the call below.
            list(SUBLIST key_and_values 1 -1 group_values)
            mayaHydra_parse_disabled_on_group(group_matches issues "${group_values}")
            if(group_matches)
                list(APPEND collect_labels "disabledOnCurrentConfig" ${issues})
            endif()
        elseif(key STREQUAL "disabled")
            list(LENGTH key_and_values key_and_values_length)
            if(key_and_values_length LESS 2)
                message(FATAL_ERROR "Tag group \"${item}\" has no tracking issue")
            endif()
            list(SUBLIST key_and_values 1 -1 group_values)
            mayaHydra_parse_disabled_group(issues "${group_values}")
            list(APPEND collect_labels "disabled" ${issues})
        else()
            # find_labels only writes its output when the group has values.
            set(label_list "")
            find_labels(${item} label_list)
            list(APPEND collect_labels ${label_list})
        endif()
    endforeach()
    set(${all_labels} ${collect_labels} PARENT_SCOPE)
endfunction()
