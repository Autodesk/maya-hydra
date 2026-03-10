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
import pathlib
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

def imageDiff(imagePath1, imagePath2, verbose, fail, failpercent, hardfail, 
                warn, warnpercent, hardwarn, perceptual):    
    """ Returns the completed process instance after running idiff or None if
        execution of process failed.
    
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

    try:
        # Run idiff command
        #
        # On some Windows 11 machines we were randomly getting a failure when
        # launching the subprocess.run().
        #   OSError: [WinError 50] The request is not supported
        #
        # The cause appeared to come from the subprocess.run() call where it
        # was only capturing stdout. In subprocess the error occured when trying
        # to duplicate the stderr handle.
        #proc = subprocess.run(cmd, shell=False, env=os.environ.copy(), stdout=subprocess.PIPE)
        # When using flag 'capture_output=True' to capture both (stdout/stderr) the
        # random error disappeared.
        proc = subprocess.run(cmd, capture_output=True, shell=False, env=os.environ.copy())
    except OSError as e:
        # If its not the random WinError 50 we re-raise it.
        if '[WinError 50]' not in str(e):
            raise

        if verbose:
            sys.__stdout__.write('\nimageDiff failed with: {0}'.format(str(e)))
            sys.__stdout__.flush()
    else:
        # Successfully executed imageDiff.
        return proc

    return None # Running of imageDiff failed.

def _getMayaScriptEditorHistoryTail(max_lines=200):
    """Return the tail of Maya Script Editor history as a string, or empty if unavailable."""
    try:
        if not cmds.scriptEditorInfo(q=True, writeHistory=True):
            return ""
        hist_file = cmds.scriptEditorInfo(q=True, historyFilename=True) or ""
        if not hist_file or not os.path.isfile(hist_file):
            return ""
        with open(hist_file, "r", encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
        tail = lines[-max_lines:] if len(lines) > max_lines else lines
        if not tail:
            return ""
        return "\n".join(
            ["", "===== Maya Script Editor history (last {} lines) =====".format(len(tail))]
            + tail
            + ["===== end Maya Script Editor history ====="]
        )
    except Exception:
        return ""

def _generateDiffImage(imagePath1, imagePath2, outputPath):
    """Generate a visual diff image using idiff -o -abs (raw pixel-by-pixel diff, no scaling). Returns output path if successful, else None."""
    image_diff_tool = os.environ.get('IMAGE_DIFF_TOOL')
    if not image_diff_tool:
        return None
    os.makedirs(os.path.dirname(outputPath), exist_ok=True)
    cmd = [image_diff_tool, '-o', outputPath, '-abs', imagePath1, imagePath2]
    try:
        proc = subprocess.run(cmd, capture_output=True, shell=False, env=os.environ.copy())
        if proc.returncode in (0, 1, 2) and os.path.isfile(outputPath):
            return outputPath
    except OSError:
        pass
    return None

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
        if proc is None:
            self.fail('Failed to execute imageDiff')

        #Enable undo again
        cmds.undoInfo(stateWithoutFlush=True)
        if proc.returncode not in (0, 1):
            abs1 = os.path.abspath(imagePath1).replace('\\', '/')
            abs2 = os.path.abspath(imagePath2).replace('\\', '/')

            artifact_base = os.environ.get('JENKINS_ARTIFACT_BASE', '').rstrip('/')
            # Prefer explicit workspace; fall back to WORKSPACE (Jenkins); then infer from paths
            workspace = os.environ.get('JENKINS_ARTIFACT_WORKSPACE', '') or os.environ.get('WORKSPACE', '')
            if workspace:
                workspace = os.path.normpath(workspace)

            if artifact_base:
                def _resolve(p):
                    try:
                        return str(pathlib.Path(p).resolve())
                    except (OSError, RuntimeError):
                        return os.path.normpath(p.replace('/', os.sep))

                def _artifact_url(abs_path, workspace_dir):
                    if not workspace_dir:
                        return None
                    try:
                        resolved_path = _resolve(abs_path)
                        resolved_ws = _resolve(workspace_dir)
                        rel = os.path.relpath(resolved_path, resolved_ws)
                        if rel.startswith('..'):
                            return None
                        return artifact_base + '/' + rel.replace('\\', '/')
                    except ValueError:
                        return None

                # Try workspace first; if relpath fails (e.g. different drives on Windows),
                # use common ancestor of both image paths as fallback workspace
                url1 = _artifact_url(abs1, workspace) if workspace else None
                url2 = _artifact_url(abs2, workspace) if workspace else None
                if (url1 is None or url2 is None) and abs1 and abs2:
                    try:
                        r1 = _resolve(abs1)
                        r2 = _resolve(abs2)
                        common = os.path.commonpath([r1, r2])
                        if common:
                            if url1 is None:
                                url1 = _artifact_url(abs1, common)
                            if url2 is None:
                                url2 = _artifact_url(abs2, common)
                    except (ValueError, OSError):
                        pass

                browse_url = artifact_base + '/'
                diff_path = None
                base2, ext2 = os.path.splitext(os.path.basename(abs2))
                diff_output = os.path.join(os.path.dirname(abs2), base2 + '_diff' + (ext2 or '.png'))
                diff_path = _generateDiffImage(abs1, abs2, diff_output)
                if diff_path:
                    diff_abs = os.path.abspath(diff_path).replace('\\', '/')
                    url_diff = _artifact_url(diff_abs, workspace) if workspace else None
                    if url_diff is None and abs1 and abs2:
                        try:
                            r_diff = _resolve(diff_abs)
                            r1 = _resolve(abs1)
                            r2 = _resolve(abs2)
                            common = os.path.commonpath([r1, r2])
                            if common:
                                url_diff = _artifact_url(diff_abs, common)
                        except (ValueError, OSError):
                            pass
                else:
                    url_diff = None

                msg = str(proc.stdout) + "\n\nImage comparison failed.\n"
                if url1:
                    msg += "  Baseline: {}\n".format(url1)
                if url2:
                    msg += "  Actual:   {}\n".format(url2)
                if url_diff:
                    msg += "  Diff:     {}\n".format(url_diff)
                msg += "  Browse:   {}\n".format(browse_url)
            else:
                msg = str(proc.stdout) + "\n\nImage comparison failed.\n"
                base2, ext2 = os.path.splitext(os.path.basename(abs2))
                diff_output = os.path.join(os.path.dirname(abs2), base2 + '_diff' + (ext2 or '.png'))
                diff_path = _generateDiffImage(abs1, abs2, diff_output)
                if diff_path:
                    msg += "  Diff:     {}\n".format(os.path.abspath(diff_path).replace('\\', '/'))

            script_editor_tail = _getMayaScriptEditorHistoryTail(max_lines=200)
            if script_editor_tail:
                msg += script_editor_tail + "\n"
            self.fail(msg)
        return proc.returncode
    
    def assertImagesEqual(self, imagePath1, imagePath2):
        self.assertImagesClose(imagePath1, imagePath2, fail=None, failpercent=None)
    
    def assertSnapshotClose(self, refImagePath, fail, failpercent, hardfail=None, 
                warn=None, warnpercent=None, hardwarn=None, perceptual=False, *, imageVersion=None):
        if imageVersion is not None:
            snapImagePath = os.path.join(self.getSnapshotDir(), imageVersion, os.path.basename(refImagePath))
        else:
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
