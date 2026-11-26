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
# Maya plug-in of the external shape adapter example: the MhExternalShapeAdapterExample
# node, a locator that draws nothing by itself. Viewport 2.0 shows nothing for
# it; Hydra shows what the example's adapter translates it to. See the README.
import maya.api.OpenMaya as om
import maya.api.OpenMayaUI as omui


def maya_useNewAPI():
    """Tell Maya this plug-in uses the Python API 2.0."""
    pass


class MhExternalShapeAdapterExample(omui.MPxLocatorNode):
    # The node type the adapter is registered for.
    kTypeName = "MhExternalShapeAdapterExample"
    kTypeId = om.MTypeId(0x58000996)

    @staticmethod
    def creator():
        return MhExternalShapeAdapterExample()

    @staticmethod
    def initialize():
        pass


def initializePlugin(obj):
    plugin = om.MFnPlugin(obj, "Autodesk", "1.0", "Any")
    plugin.registerNode(MhExternalShapeAdapterExample.kTypeName,
                        MhExternalShapeAdapterExample.kTypeId,
                        MhExternalShapeAdapterExample.creator,
                        MhExternalShapeAdapterExample.initialize,
                        om.MPxNode.kLocatorNode)


def uninitializePlugin(obj):
    plugin = om.MFnPlugin(obj)
    plugin.deregisterNode(MhExternalShapeAdapterExample.kTypeId)
