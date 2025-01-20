#!/usr/bin/env python

#
# Copyright 2019 Autodesk
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

"""
    Helper functions regarding Maya that will be used throughout the test.
"""

from math import radians

try:
    from mayaUsd import lib as mayaUsdLib
    from mayaUsd import ufe as mayaUsdUfe
except:
    pass

from maya import cmds
from maya import mel
from maya.api import OpenMaya as om

import ufe
import ufeUtils, testUtils

import os
import sys

mayaSeparator = "|"

HD_STORM = "HdStormRendererPlugin"
HD_STORM_OVERRIDE = "mayaHydraRenderOverride_" + HD_STORM

def createUfePathSegment(mayaPath):
    """
        Create a UFE path from a given maya path and return the first segment.
        Args:
            mayaPath (str): The maya path to use
        Returns :
            PathSegment of the given mayaPath
    """
    if ufeUtils.ufeFeatureSetVersion() >= 2:
        return ufe.PathString.path(mayaPath).segments[0]
    else:
        if not mayaPath.startswith("|world"):
            mayaPath = "|world" + mayaPath
        return ufe.PathSegment(mayaPath, mayaUsdUfe.getMayaRunTimeId(),
            mayaSeparator)

def getMayaSelectionList():
    """ 
        Returns the current Maya selection in a list
        Returns:
            A list(str) containing all selected Maya items
    """
    # Remove the unicode of cmds.ls

    # TODO: HS, June 10, 2020 investigate why x needs to be encoded
    if sys.version_info[0] == 2:
        return [x.encode('UTF8') for x in cmds.ls(sl=True)]
    else:
        return [x for x in cmds.ls(sl=True)]

def isHydraRenderer():
    activeEditor = cmds.playblast(activeEditor=1)
    activeRenderer = cmds.modelEditor(activeEditor, q=True,rendererOverrideName=True)
    return activeRenderer == HD_STORM_OVERRIDE

def resetDefaultLightIntensity():
    """If the current Maya version supports setting the default light intensity,
        then restore it to 1 so snapshots look equal across versions."""
    if mel.eval("optionVar -exists defaultLightIntensity"):
        mel.eval("optionVar -fv defaultLightIntensity 1")
    if cmds.attributeQuery('defaultLightIntensity', node='hardwareRenderingGlobals', exists=True):
        cmds.setAttr('hardwareRenderingGlobals.defaultLightIntensity', 1.0)

def applyTestSettings():
    resetDefaultLightIntensity()
    cmds.headsUpDisplay(layoutVisibility=False)
    cmds.grid(toggle=False)
    cmds.colorManagementPrefs(edit=True, cmEnabled=False)
    cmds.setAttr("hardwareRenderingGlobals.multiSampleEnable", True) # TODO : Turn anti-aliasing off by default.

def openNewScene(useTestSettings=True):
    cmds.file(new=True, force=True)
    if useTestSettings:
        applyTestSettings()
        # As this conceptually opens a new scene, set the file to unmodified.
        cmds.file(modified=False)

def openTestScene(*args, useTestSettings=True):
    filePath = testUtils.getTestScene(*args)
    cmds.file(filePath, force=True, open=True)
    if useTestSettings:
        applyTestSettings()

def setMayaTranslation(aMayaItem, t):
    '''Set the translation on the argument Maya scene item.'''

    aMayaPath = aMayaItem.path()
    aMayaPathStr = ufe.PathString.string(aMayaPath)
    aDagPath = om.MSelectionList().add(aMayaPathStr).getDagPath(0)
    aFn= om.MFnTransform(aDagPath)
    aFn.setTranslation(t, om.MSpace.kObject)
    return (aMayaPath, aMayaPathStr, aFn, aFn.transformation().asMatrix())

def setMayaRotation(aMayaItem, r):
    '''Set the rotation (XYZ) on the argument Maya scene item.'''

    aMayaPath = aMayaItem.path()
    aMayaPathStr = ufe.PathString.string(aMayaPath)
    aDagPath = om.MSelectionList().add(aMayaPathStr).getDagPath(0)
    aFn = om.MFnTransform(aDagPath)
    rads = [ radians(v) for v in r ]
    rot = om.MEulerRotation(rads[0], rads[1], rads[2])
    aFn.setRotation(rot, om.MSpace.kTransform)
    return (aMayaPath, aMayaPathStr, aFn, aFn.transformation().asMatrix())

def createProxyAndStage():
    """
    Create in-memory stage
    """
    cmds.createNode('mayaUsdProxyShape', name='stageShape')

    shapeNode = cmds.ls(sl=True,l=True)[0]
    shapeStage = mayaUsdLib.GetPrim(shapeNode).GetStage()
    
    cmds.select( clear=True )
    cmds.connectAttr('time1.outTime','{}.time'.format(shapeNode))

    return shapeNode,shapeStage

def createProxyFromFile(filePath):
    """
    Load stage from file
    """
    cmds.createNode('mayaUsdProxyShape', name='stageShape')

    shapeNode = cmds.ls(sl=True,l=True)[0]
    cmds.setAttr('{}.filePath'.format(shapeNode), filePath, type='string')
    
    shapeStage = mayaUsdLib.GetPrim(shapeNode).GetStage()
    
    cmds.select( clear=True )
    cmds.connectAttr('time1.outTime','{}.time'.format(shapeNode))

    return shapeNode,shapeStage

def mayaMajorVersion():
    return int(cmds.about(majorVersion=True))

def mayaMinorVersion():
    return int(cmds.about(minorVersion=True))

def mayaMajorMinorVersions():
    """
    Return the Maya version as a tuple (Major, Minor).
    Thanks to Python tuple comparison rules, (2022, 0) > (2021,3).
    """
    return (mayaMajorVersion(), mayaMinorVersion())

def ufeSupportFixLevel():
    '''
    Return the fix level defined in the UFE support package.  This is used
    to determine the presence of a UFE-related feature or bug fix in Maya that
    does not depend on a version of UFE itself.
    '''
    import maya.internal.ufeSupport.utils as ufeSupportUtils
    return ufeSupportUtils.fixLevel() if hasattr(ufeSupportUtils, 'fixLevel') \
        else 0

def hydraFixLevel():
    '''
    Return the Hydra fix level defined in Maya.

    This is used to determine the presence of a Hydra-related feature or bug 
    fix in Maya.
    '''
    import maya.internal.ufeSupport.utils as ufeSupportUtils
    return ufeSupportUtils.hydraFixLevel() if hasattr(ufeSupportUtils, 'hydraFixLevel') \
        else 0

def activeModelPanel():
    """Return the model panel that will be used for playblasting etc..."""
    for panel in cmds.getPanel(type="modelPanel"):
        if cmds.modelEditor(panel, q=1, av=1):
            return panel
