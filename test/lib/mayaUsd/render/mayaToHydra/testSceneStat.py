# Copyright 2025 Autodesk
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

import maya.cmds as cmds
import maya.mel as mel
import unittest

import fixturesUtils
import mtohUtils

class TestSceneStatistics(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    def test_sceneStatistics(self):
        # empty scene, no primitives
        cmds.file(new=True, force=True)
        cmds.refresh()
        empty_stats = cmds.mayaHydra(sceneStats=True)
        empty_info = {empty_stats[i]: int(empty_stats[i + 1]) for i in range(0, len(empty_stats), 2)}
        
        # simple primitive scene -- 1 mesh, curve, point
        cmds.polyCube()
        cmds.circle() 
        cmds.particle(position=[(0, 0, 0)])
        cmds.refresh()
        primitive_stats = cmds.mayaHydra(sceneStats=True)
        primitive_info = {primitive_stats[i]: int(primitive_stats[i + 1]) for i in range(0, len(primitive_stats), 2)}

        expected_empty_counts = {
            "primitives": 0,
            "mesh": 0,
            "mesh.points": 0,
            "mesh.faces": 0,
            "curve": 0,
            "curve.points": 0,
            "point": 0,
        }
        
        expected_primitive_counts = {
            "primitives": 3,
            "mesh": 1,
            "mesh.points": 24,
            "mesh.faces": 12,
            "curve": 1,
            "curve.points": 41,
            "point": 1,
        }
        
        for stat_type, expected_count in expected_empty_counts.items():
            actual_count = empty_info.get(stat_type, 0)
            self.assertEqual(actual_count, expected_count, 
                           f"Empty scene {stat_type} should be {expected_count}, got {actual_count}")
        
        for stat_type, expected_count in expected_primitive_counts.items():
            actual_count = primitive_info.get(stat_type, 0)
            self.assertEqual(actual_count, expected_count, 
                           f"{stat_type} count should be {expected_count}, got {actual_count}")

if __name__ == '__main__':
    fixturesUtils.runTests(globals()) 
