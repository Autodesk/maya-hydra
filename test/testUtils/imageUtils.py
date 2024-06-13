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
import subprocess

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

def imageDiff(imagePath1, imagePath2, verbose, fail, failpercent, hardfail, 
                warn, warnpercent, hardwarn, perceptual):    
    """ Returns the completed process instance after running idiff.
    
    imagePath1   -- First image to compare.
    imagePath2   -- Second image to compare.
    verbose      -- If enabled, the image diffing command will be printed to log.
    fail         -- The threshold for the acceptable difference (relatively to the mean of 
                    the two values) of a pixel for failure.    
    failpercent  -- The percentage of pixels that can be different before failure.
    hardfail     -- Triggers a failure if any pixels are above this threshold (if the absolute 
                    difference is below this threshold).
    warn         -- The threshold for the acceptable difference of a pixel for a warning.
    warnpercent  -- The percentage of pixels that can be different before a warning.
    hardwarn     -- Triggers a warning if any pixels are above this threshold.
    perceptual   -- Performs an additional test to see if two images are visually different.
                    If enabled, test overall will fail if more than the "fail percentage" failed 
                    the perceptual test.
    
    By default, if any pixels differ between the images, the comparison will fail.
    If, for example, we set fail=0.004, failpercent=10 and hardfail=0.25, the comparison will 
    fail if more than 10% of the pixels differ by 0.004, or if any pixel differs by more than 
    0.25 (just above a 1/255 threshold).
     
    For more information, see https://github.com/OpenImageIO/oiio/blob/cb6475c0dd72b9c49d862d98c6cd2da4509d5f37/src/doc/idiff.rst#L1
    """
    import platform

    imageDiff = os.environ['IMAGE_DIFF_TOOL']
    
    cmdArgs = []
    if warn is not None:
        cmdArgs.extend(['-warn', str(warn)])
    if warnpercent is not None:
        cmdArgs.extend(['-warnpercent', str(warnpercent)])
    if hardwarn is not None:
        cmdArgs.extend(['-hardwarn', str(hardwarn)])
    if fail is not None:
        cmdArgs.extend(['-fail', str(fail)])
    if failpercent is not None:
        cmdArgs.extend(['-failpercent', str(failpercent)])
    if hardfail is not None:
        cmdArgs.extend(['-hardfail', str(hardfail)])
    if perceptual:
        cmdArgs.extend(['-p'])
    cmd = [imageDiff]
    cmd.extend(cmdArgs)
    cmd.extend([imagePath1, imagePath2])
    
    if verbose:
        import sys
        sys.__stdout__.write("\nimage diffing with {0}".format(cmd))
        sys.__stdout__.flush()

    # Run idiff command
    proc = subprocess.run(cmd, shell=False, env=os.environ.copy(), stdout=subprocess.PIPE)
    return proc

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
                    warn=None, warnpercent=None, hardwarn=None, perceptual=False):
        """ 
        The method will return idiff's return code if the comparison passes with 
        a return code of 0 or 1. 
        0 -- OK: the images match within the warning and error thresholds.
        1 -- Warning: the errors differ a little, but within error thresholds.
        
        The assertion will fail if the return code is 2, 3 or 4.
        2 -- Failure: the errors differ a lot, outside error thresholds.
        3 -- The images were not the same size and could not be compared.
        4 -- File error: could not find or open input files, etc.
        """
        #Disable undo
        cmds.undoInfo(stateWithoutFlush=False)
        proc = imageDiff(imagePath1, imagePath2, verbose=True, 
                            fail=fail, failpercent=failpercent, hardfail=hardfail,
                            warn=warn, warnpercent=warnpercent, hardwarn=hardwarn, 
                            perceptual=perceptual)
        #Enable undo again
        cmds.undoInfo(stateWithoutFlush=True)
        if proc.returncode not in (0, 1):
            self.fail(str(proc.stdout))
        return proc.returncode
    
    def assertImagesEqual(self, imagePath1, imagePath2):
        self.assertImagesClose(imagePath1, imagePath2, fail=None, failpercent=None)
    
    def assertSnapshotClose(self, refImagePath, fail, failpercent, hardfail=None, 
                warn=None, warnpercent=None, hardwarn=None, perceptual=False):
        snapImagePath = os.path.join(self.getSnapshotDir(), os.path.basename(refImagePath))
        snapshot(snapImagePath)
        
        return self.assertImagesClose(refImagePath, snapImagePath, 
               fail=fail, failpercent=failpercent, hardfail=hardfail,
               warn=warn, warnpercent=warnpercent, hardwarn=hardwarn, 
               perceptual=perceptual)
        
    def assertSnapshotEqual(self, refImagePath):
        '''Use of this method is discouraged, as renders can vary slightly between renderer architectures.'''
        return self.assertSnapshotClose(refImagePath, fail=None, failpercent=None)

    def assertSnapshotSilhouetteClose(self, refImagePath, fail, failpercent, hardfail=None, 
                warn=None, warnpercent=None, hardwarn=None, perceptual=False):
        refImageName, refImageExtension = os.path.splitext(os.path.basename(refImagePath))

        refSilhouetteImagePath = os.path.join(self.getSnapshotDir(), refImageName + "_ReferenceSilhouette" + refImageExtension)
        shutil.copy(refImagePath, refSilhouetteImagePath)
        convertToSilhouette(refSilhouetteImagePath)        

        snapSilhouetteImagePath = os.path.join(self.getSnapshotDir(), refImageName + "_SnapshotSilhouette" + refImageExtension)
        snapshot(snapSilhouetteImagePath)
        convertToSilhouette(snapSilhouetteImagePath)

        return self.assertImagesClose(refSilhouetteImagePath, snapSilhouetteImagePath, 
               fail=fail, failpercent=failpercent, hardfail=hardfail,
               warn=warn, warnpercent=warnpercent, hardwarn=hardwarn, 
               perceptual=perceptual)
