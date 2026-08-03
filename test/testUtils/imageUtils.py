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
import platform
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

# Minimum mean Rec.709 luminance (0-1) for hydraSnapshot captures. Catches blank
# PRMan/playblast failures where geometry and lighting never reach the AOV.
SNAPSHOT_MIN_MEAN_LUMINANCE = 0.02


# Context manager that routes viewport output through ImageBufferWriter.
#
# Playblast reads Maya's model-editor framebuffer. Progressive delegates such as
# PRMan render into Hydra AOVs instead, so playblast can be blank locally even
# when the viewport looks correct. ImageBufferWriter reads the same Hydra buffer
# the override presents. Requires the mayaHydraCppTests plugin (TestWriteFile.*).
#
# __init__ parameters:
#   outputFile -- path for the captured image (TestWriteFile.setFileName).
#   width      -- capture width in pixels (TestWriteFile.setImageSize).
#   height     -- capture height in pixels (TestWriteFile.setImageSize).
#
# Usage:
#   with WriteFile(output_path, 400, 400):
#       cmds.refresh(force=True)
class WriteFile(object):

    def __init__(self, outputFile, width, height):
        self.outputFile = outputFile
        self.width = width
        self.height = height

    def __enter__(self):
        import maya.cmds as cmds
        cmds.mayaHydraCppTest(self.outputFile, f="TestWriteFile.setFileName")
        cmds.mayaHydraCppTest(self.width, self.height, f="TestWriteFile.setImageSize")
        # setImageSize resizes the render buffers, which restarts progressive
        # delegates (e.g. PRMan) from zero samples at the new resolution. Any
        # settling done before this resize is for the old resolution and does
        # not carry over. Callers that need settle time should use hydraSnapshot's
        # settleFn(captureRefresh) so settling and the final capture happen
        # after the resize, in the right order.
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        import maya.cmds as cmds
        cmds.mayaHydraCppTest(f="TestWriteFile.unsetImageSize")
        cmds.mayaHydraCppTest("", f="TestWriteFile.setFileName")
        return False


# Capture the Hydra viewport color AOV to an image file.
#
# Prefer over snapshot()/playblast for progressive Hydra delegates (e.g. PRMan)
# that render into AOVs rather than the model-editor framebuffer.
#
# Parameters:
#   outputPath -- destination image path; parent dirs created; made absolute.
#   width      -- capture width in pixels (default 400).
#   height     -- capture height in pixels (default: same as width).
#   settleFn   -- optional callable(captureRefresh) invoked after buffer resize.
#                 Must call captureRefresh() exactly once for the file write.
#                 For PRMan: poll for convergence (refreshing each iteration),
#                 then captureRefresh() (see _settlePrmanBeforeSnapshot).
#                 hydraSnapshot never adds a second capture refresh when
#                 settleFn is provided.
#
# Usage:
#   hydraSnapshot(snap_path, settleFn=self._settlePrmanBeforeSnapshot)
def hydraSnapshot(outputPath, width=400, height=None, settleFn=None):
    import maya.cmds as cmds

    if height is None:
        height = width

    outputPath = os.path.abspath(outputPath)
    os.makedirs(os.path.dirname(outputPath), exist_ok=True)

    def _captureRefresh():
        cmds.refresh(force=True)

    cmds.undoInfo(stateWithoutFlush=False)
    try:
        with WriteFile(outputPath, width, height):
            if settleFn:
                settleFn(_captureRefresh)
            else:
                _captureRefresh()
    finally:
        cmds.undoInfo(stateWithoutFlush=True)


# Capture the active model editor via playblast.
#
# Parameters:
#   outputPath -- destination image path; parent dirs created; made absolute.
#   width      -- playblast width in pixels (default 400).
#   height     -- playblast height in pixels (default: same as width).
#
# Usage:
#   snapshot(snap_path)
def snapshot(outputPath, width=400, height=None):
    import maya.cmds as cmds

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

# Return mean Rec.709 luminance (0-1) over opaque pixels in an image file.
#
# Parameters:
#   imagePath -- readable image path (.png, .jpg, etc.).
#
# Usage:
#   mean = imageMeanLuminance(snap_path)
def imageMeanLuminance(imagePath):
    import struct

    from PySide6.QtGui import QImage

    image = QImage(imagePath)
    if image.isNull():
        raise RuntimeError("Failed to load image: {}".format(imagePath))

    # Unpack the raw RGBA8888 buffer instead of calling QImage.pixelColor()
    # once per pixel: pixelColor() allocates a QColor and crosses the
    # Python/C++ binding on every call, which dominates wall time at typical
    # capture resolutions (e.g. ~0.25s at 400x400) -- this runs on every
    # PRMan light capture, directly adding to test time. RGBA8888 is 4
    # bytes/pixel, so each scanline is exactly width*4 bytes with no padding
    # and the whole buffer can be unpacked in one pass.
    image = image.convertToFormat(QImage.Format.Format_RGBA8888)
    pixelCount = image.width() * image.height()
    byteCount = image.sizeInBytes()
    ptr = image.constBits()
    # Older PySide6 builds return a Shiboken buffer without an implicit size;
    # setsize() must be called before bytes() or the buffer may be empty/short.
    if hasattr(ptr, "setsize"):
        ptr.setsize(byteCount)
    buf = bytes(ptr)[:pixelCount * 4]

    total = 0.0
    count = 0
    for r, g, b, a in struct.iter_unpack("4B", buf):
        if a:
            total += 0.2126 * r + 0.7152 * g + 0.0722 * b
            count += 1
    if count == 0:
        return 0.0
    return (total / count) / 255.0

# Fail when a snapshot is effectively blank (e.g. empty Hydra AOV / playblast miss).
#
# Parameters:
#   imagePath            -- captured snapshot to inspect.
#   min_mean_luminance   -- minimum acceptable mean luminance (default
#                           SNAPSHOT_MIN_MEAN_LUMINANCE).
#
# Usage:
#   assertImageNonBlank(snap_path)
def assertImageNonBlank(imagePath, min_mean_luminance=SNAPSHOT_MIN_MEAN_LUMINANCE):
    mean = imageMeanLuminance(imagePath)
    if mean < min_mean_luminance:
        raise AssertionError(
            "Snapshot appears blank (mean luminance {:.4f} < {:.4f}): {}".format(
                mean, min_mean_luminance, imagePath
            )
        )

def imageDiff(imagePath1, imagePath2, verbose, fail, failpercent, hardfail=None,
                failrelative=None, warn=None, warnpercent=None, hardwarn=None,
                perceptual=False):    
    """ Returns the completed process instance after running idiff or None if
        execution of process failed.
    
    imagePath1   -- First image to compare.
    imagePath2   -- Second image to compare.
    verbose      -- If enabled, the image diffing command will be printed to log.
    fail         -- The threshold for absolute pixel difference for failure.
    failpercent  -- The percentage of pixels that can be different before failure.
    failrelative -- If set, uses relative difference (scaled by mean of two values). Use 0 for
                    strict pixel-per-pixel absolute comparison only.
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
    if failrelative is not None:
        cmdArgs.extend(['-failrelative', str(failrelative)])
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
        #
        # On Windows 11 24H2 (Windows Terminal as the default console host),
        # launching a console child like idiff.exe from a Maya UI process (which
        # has no inherited console) hangs subprocess.run forever -- Windows
        # fails the console-pipe handshake with ERROR_NO_DATA (0x800700E8).
        # CREATE_NO_WINDOW skips that handshake.
        creation_flags = subprocess.CREATE_NO_WINDOW if platform.system() == "Windows" else 0
        proc = subprocess.run(
            cmd,
            capture_output=True,
            shell=False,
            env=os.environ.copy(),
            creationflags=creation_flags,
        )
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

def _generateDiffImage(imagePath1, imagePath2, outputPath):
    """Generate a visual diff image. Prefers oiiotool --absdiff --maxchan; falls back to idiff.
    Returns output path if successful, else None."""
    import sys
    image_diff_tool = os.environ.get('IMAGE_DIFF_TOOL')
    if not image_diff_tool:
        return None
    os.makedirs(os.path.dirname(outputPath), exist_ok=True)

    # Prefer oiiotool: --absdiff --maxchan --powc 0.5 --mulc 40 --clamp:max=1
    oiiotool_exe = (
        os.environ.get('OIIOTOOL') or
        (os.path.join(os.path.dirname(image_diff_tool),
          'oiiotool.exe' if sys.platform == 'win32' else 'oiiotool')
         if os.path.dirname(image_diff_tool) else None) or
        (shutil.which('oiiotool') if shutil.which else None)
    )
    if oiiotool_exe and os.path.isfile(oiiotool_exe):
        cmd = [
            oiiotool_exe, imagePath1, imagePath2,
            '--absdiff', '--maxchan', '--powc', '0.5', '--mulc', '40',
            '--clamp:max=1', '-o', outputPath
        ]
        env = os.environ.copy()
        oiio_dir = os.path.dirname(oiiotool_exe)
        if oiio_dir:
            env['PATH'] = oiio_dir + os.pathsep + env.get('PATH', '')
        try:
            proc = subprocess.run(cmd, capture_output=True, shell=False, env=env)
            if proc.returncode == 0 and os.path.isfile(outputPath):
                return outputPath
        except OSError:
            pass

    # Fallback: idiff -o -abs -scale 1
    cmd = [image_diff_tool, '-o', outputPath, '-abs', '-scale', '1', imagePath1, imagePath2]
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
        """
        import maya.cmds as cmds

        #Disable undo
        cmds.undoInfo(stateWithoutFlush=False)
        proc = imageDiff(imagePath1, imagePath2, verbose=True, 
                            fail=fail, failpercent=failpercent, hardfail=hardfail,
                            failrelative=failrelative,
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
                _, ext1 = os.path.splitext(os.path.basename(abs1))
                base2, ext2 = os.path.splitext(os.path.basename(abs2))
                ext = ext1 or ext2 or '.png'
                diff_output = os.path.join(os.path.dirname(abs2), base2 + '_diff' + ext)
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
                msg += "  Baseline: {}\n".format(abs1)
                msg += "  Actual:   {}\n".format(abs2)
                _, ext1 = os.path.splitext(os.path.basename(abs1))
                base2, ext2 = os.path.splitext(os.path.basename(abs2))
                ext = ext1 or ext2 or '.png'
                diff_output = os.path.join(os.path.dirname(abs2), base2 + '_diff' + ext)
                diff_path = _generateDiffImage(abs1, abs2, diff_output)
                if diff_path:
                    msg += "  Diff:     {}\n".format(os.path.abspath(diff_path).replace('\\', '/'))

            self.fail(msg)
        return proc.returncode
    
    def assertImagesEqual(self, imagePath1, imagePath2):
        self.assertImagesClose(imagePath1, imagePath2, fail=None, failpercent=None)
    
    # Capture the viewport, then compare against a baseline image.
    #
    # Takes a snapshot into the test temp dir and runs idiff against refImagePath.
    # Capture options are forwarded to snapshot() or hydraSnapshot().
    #
    # Capture-related parameters:
    #   useHydraWriter -- when True, capture via hydraSnapshot (required for PRMan).
    #   hydraSettleFn  -- settleFn(captureRefresh) for hydraSnapshot when
    #                     useHydraWriter is True; must call captureRefresh once.
    # When useHydraWriter is True, assertImageNonBlank runs on the capture first.
    #
    # Usage:
    #   self.assertSnapshotClose("Storm_pointLight1.png", 0.1, 7.0)
    #   self.assertSnapshotClose("PRMan_pointLight1.png", 0.2, 10.0,
    #       useHydraWriter=True, hydraSettleFn=self._settlePrmanBeforeSnapshot)
    def assertSnapshotClose(self, refImagePath, fail, failpercent, hardfail=None, 
                failrelative=None, warn=None, warnpercent=None, hardwarn=None,
                perceptual=False, *, imageVersion=None,
                useHydraWriter=False, hydraSettleFn=None):
        if imageVersion is not None:
            snapImagePath = os.path.join(self.getSnapshotDir(), imageVersion, os.path.basename(refImagePath))
        else:
            snapImagePath = os.path.join(self.getSnapshotDir(), os.path.basename(refImagePath))
        if useHydraWriter:
            hydraSnapshot(snapImagePath, settleFn=hydraSettleFn)
            assertImageNonBlank(snapImagePath)
        else:
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
