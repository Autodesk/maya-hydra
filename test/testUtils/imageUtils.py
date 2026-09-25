# Copyright 2020 Luma Pictures
# Copyright 2023 Autodesk
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
import os
import maya.cmds as cmds
import shutil

import imageDiffUtils

KNOWN_FORMATS = {
    'gif': 0,
    'tif': 3,
    'tiff': 3,
    'sgi': 5,
    'iff': 7,
    'jpg': 8,
    'jpeg': 8,
    'tga': 19,
    'bmp': 20,
    'png': 32,
}

def snapshot(outputPath, width=400, height=None):
    #Disable undo so that when we call undo it doesn't undo any operation from self.assertSnapshotClose
    cmds.undoInfo(stateWithoutFlush=False)

    if height is None:
        height = width

    outputExt = os.path.splitext(outputPath)[1].lower().lstrip('.')

    formatNum = KNOWN_FORMATS.get(outputExt)
    if formatNum is None:
        raise ValueError("input image had unrecognized extension: {}"
                         .format(outputExt))

    # if given relative path, make it relative to current dir (the test
    # temp base), rather than the workspace dir
    outputPath = os.path.abspath(outputPath)
    os.makedirs(os.path.dirname(outputPath), exist_ok=True)

    # save the old output image format
    oldFormat = cmds.getAttr("defaultRenderGlobals.imageFormat")

    cmds.setAttr("defaultRenderGlobals.imageFormat", formatNum)
    try:
        cmds.refresh()
        cmds.playblast(cf=outputPath, viewer=False, format="image",
                       frame=cmds.currentTime(q=1), offScreen=1,
                       widthHeight=(width, height), percent=100)
    finally:
        cmds.setAttr("defaultRenderGlobals.imageFormat", oldFormat)

    #Enable undo again
    cmds.undoInfo(stateWithoutFlush=True)

def convertToSilhouette(imagePath):
    # 2024-06-13 : Tried to use oiiotool instead of PySide for this to be more efficient,
    # however it did not work under certain circumstances, for example when trying to
    # run it on the copied reference image assertSnapshotSilhouetteClose. It did work on
    # captured snapshots for some reason. The error oiiotool gave out was something like
    # "Could not open file someFileName.[randomAlphaNumericCharacters].temp.[png|jpg]".
    # The temp file mentioned is itself being created by OIIO. Attaching a debugger is not
    # straightforward at least on Windows, as the process is started by the tests, and
    # trying to run oiiotool independently did not work due to not finding boost libs.
    # Decided not to investigate further for now, as this is an implementation detail
    # that can always be swapped out later if we need the performance, and it is not
    # going to be used in most cases.
    from PySide6.QtGui import QImage, QColor

    image = QImage(imagePath)

    for x in range(image.width()):
        for y in range(image.height()):
            if image.pixelColor(x, y).alpha() > 0:
                image.setPixelColor(x, y, QColor(255, 255, 255, 255))

    image.save(imagePath)

class ImageDiffingTestCase:
    '''Mixin class for unit tests that require image comparison.'''

    def getSnapshotDir(self):
        snapshotDir = os.path.join(os.path.abspath('.'), self._testMethodName)
        if not os.path.isdir(snapshotDir):
            os.makedirs(snapshotDir)
        return snapshotDir

    def assertImagesClose(self, imagePath1, imagePath2, fail, failpercent, hardfail=None,
                    failrelative=None, warn=None, warnpercent=None, hardwarn=None,
                    perceptual=False):
        """
        The method will return idiff's return code if the comparison passes with
        a return code of 0 or 1.
        0 -- OK: the images match within the warning and error thresholds.
        1 -- Warning: the errors differ a little, but within error thresholds.

        The assertion will fail if the return code is 2, 3 or 4.
        2 -- Failure: the errors differ a lot, outside error thresholds.
        3 -- The images were not the same size and could not be compared.
        4 -- File error: could not find or open input files, etc.

        imagePath1 -- baseline (expected) image.
        imagePath2 -- actual (captured) image.
        """
        #Disable undo so that when we call undo it doesn't undo any operation from the caller.
        cmds.undoInfo(stateWithoutFlush=False)
        result = imageDiffUtils.compareImagePair(
            imagePath1, imagePath2, fail, failpercent, hardfail=hardfail,
            failrelative=failrelative, warn=warn, warnpercent=warnpercent,
            hardwarn=hardwarn, perceptual=perceptual, passReturnCodes=(0, 1),
            verbose=True,
        )
        #Enable undo again
        cmds.undoInfo(stateWithoutFlush=True)

        if not result.passed:
            self.fail(result.message)
        return result.returncode

    def assertImagesEqual(self, imagePath1, imagePath2):
        self.assertImagesClose(imagePath1, imagePath2, fail=None, failpercent=None)

    def assertSnapshotClose(self, refImagePath, fail, failpercent, hardfail=None,
                failrelative=None, warn=None, warnpercent=None, hardwarn=None,
                perceptual=False, *, imageVersion=None):
        if imageVersion is not None:
            snapImagePath = os.path.join(self.getSnapshotDir(), imageVersion, os.path.basename(refImagePath))
        else:
            snapImagePath = os.path.join(self.getSnapshotDir(), os.path.basename(refImagePath))
        snapshot(snapImagePath)

        return self.assertImagesClose(refImagePath, snapImagePath,
               fail=fail, failpercent=failpercent, hardfail=hardfail,
               failrelative=failrelative,
               warn=warn, warnpercent=warnpercent, hardwarn=hardwarn,
               perceptual=perceptual)

    def assertSnapshotEqual(self, refImagePath):
        '''Use of this method is discouraged, as renders can vary slightly between renderer architectures.'''
        return self.assertSnapshotClose(refImagePath, fail=None, failpercent=None)

    def assertSnapshotSilhouetteClose(self, refImagePath, fail, failpercent, hardfail=None,
                failrelative=None, warn=None, warnpercent=None, hardwarn=None,
                perceptual=False):
        refImageName, refImageExtension = os.path.splitext(os.path.basename(refImagePath))

        refSilhouetteImagePath = os.path.join(self.getSnapshotDir(), refImageName + "_ReferenceSilhouette" + refImageExtension)
        shutil.copy(refImagePath, refSilhouetteImagePath)
        convertToSilhouette(refSilhouetteImagePath)

        snapSilhouetteImagePath = os.path.join(self.getSnapshotDir(), refImageName + "_SnapshotSilhouette" + refImageExtension)
        snapshot(snapSilhouetteImagePath)
        convertToSilhouette(snapSilhouetteImagePath)

        return self.assertImagesClose(refSilhouetteImagePath, snapSilhouetteImagePath,
               fail=fail, failpercent=failpercent, hardfail=hardfail,
               failrelative=failrelative,
               warn=warn, warnpercent=warnpercent, hardwarn=hardwarn,
               perceptual=perceptual)
