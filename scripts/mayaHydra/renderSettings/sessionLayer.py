# Copyright 2026 Autodesk, Inc. All rights reserved.
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

# This module implements an important design choice: there is logic behind the
# choice of the session layer stage. If absent, it will be the
# UsdDefaultRenderDescription stage, and if present, it can be a
# MayaUsdProxyShape stage, which is represented by a Dag path. This cannot be
# handled at the Render argument parsing level in Python, where each argument
# is handled separately, so during Render argument parsing we simply copy the
# session layer and session layer stage arguments into dynamic attributes on
# the UsdDefaultRenderDescription node.
#
# These values can then later be retrieved by the hydraRender command, which
# then has the whole picture and both arguments. The dynamic attributes are
# never persisted, and serve only as a communication channel to get values from
# the Render command line into the hydraRender command.

from contextlib import contextmanager

import maya.cmds as cmds

_NODE = 'UsdDefaultRenderDescription'


@contextmanager
def _unlocked(node):
    wasLocked = cmds.lockNode(node, query=True, lock=True)[0]
    cmds.lockNode(node, lock=False)
    # As per contextmanager behavior, everything before yield runs in
    # __enter__, and everything after yield runs in __exit__.
    try:
        yield
    finally:
        cmds.lockNode(node, lock=wasLocked)


def _setAttr(attrName, value, usedAsFilename):
    if not cmds.objExists(_NODE):
        raise RuntimeError(
            "Cannot set %s: node %s not found in the scene."
            % (attrName, _NODE))

    if not cmds.attributeQuery(attrName, node=_NODE, exists=True):
        with _unlocked(_NODE):
            cmds.addAttr(_NODE, longName=attrName, dataType='string',
                         usedAsFilename=usedAsFilename)

    cmds.setAttr('%s.%s' % (_NODE, attrName), value, type='string')


def setSessionLayer(sessionLayerPath):
    # A Maya file path attribute is a string attribute flagged with
    # usedAsFilename, so that the File Path Editor and path remapping treat
    # the value as a path.
    _setAttr('sessionLayer', sessionLayerPath, usedAsFilename=True)


def setSessionLayerStage(stagePath):
    _setAttr('sessionLayerStage', stagePath, usedAsFilename=False)
