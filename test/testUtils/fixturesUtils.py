#
# Copyright 2020 Autodesk
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
import shutil
import sys
import time
import threading
import unittest

def _setUpClass(modulePathName, pluginName, initializeStandalone):
    '''
    Common code for setUpClass() and readOnlySetUpClass()
    '''
    if initializeStandalone:
        from maya import standalone
        standalone.initialize('usd')

    if pluginName:
        import maya.cmds as cmds
        wasModified = cmds.file(query=True, modified=True)
        cmds.loadPlugin(pluginName, quiet=True)
        isModified = cmds.file(query=True, modified=True)
        assert isModified == wasModified, ('Loading plugin %s modified the scene' % pluginName)

    realPath = os.path.realpath(modulePathName)
    return os.path.split(realPath)

def setUpClass(modulePathName, pluginName, initializeStandalone=True, suffix=''):
    '''
    Test class setup.

    This function:
    - (Optionally) Initializes Maya standalone use.
    - Creates (or empties) a test output directory based on the argument.
    - Changes the current working directory to the test output directory.
    - (Optionally) Loads the plugin
    - Returns the original directory from the argument.
    '''
    (testDir, testFile) = _setUpClass(modulePathName, pluginName, 
                                      initializeStandalone)
    outputName = os.path.splitext(testFile)[0]+suffix+'Output'

    outputPath = os.path.join(os.path.abspath('.'), outputName)
    if os.path.exists(outputPath):
        # Remove previous test run output.
        shutil.rmtree(outputPath)

    os.mkdir(outputPath)
    os.chdir(outputPath)

    return testDir

def tearDownClass(pluginName):
    '''
    Test class teardown.
    
    This function:
    - Changes the current working directory to the main test directory.
    - (Optionally) Unloads the plugin

    Required when running multiple test classes using fixturesUtils to
    avoid nested test directories.
    '''
    
    if pluginName:
        import maya.cmds as cmds
        cmds.unloadPlugin(pluginName, force=True)

    # Exit into the main test directory
    os.chdir("..")

def readOnlySetUpClass(modulePathName, pluginName, initializeStandalone=True):
    '''
    Test class import setup for tests that do not write to the file system.

    This function:
    - (Optionally) Initializes Maya standalone use.
    - (Optionally) Loads the plugin
    - Returns the original directory from the argument.
    '''
    (testDir, testFile) = _setUpClass(modulePathName, pluginName, 
                                      initializeStandalone)

    return testDir

def loadTestsFromDict(namespace_dict):
    '''
    Returns a unittest.TestSuite object with tests loaded from the given dict

    Similar to unittest.TestLoader.loadTestsFromModule, but works off a dict
    rather than a module object. Useful when running from inside a "script"
    context where there is no module, but are globals().

    Examples
    --------
    >>> testSuite = loadTestsFromDict(globals())
    '''
    # just piggy-back on loadTestsFromModule, since all it does is dir() and
    # getattr checks...
    class DummyModule(object):
        __name__

    dummyModule = DummyModule()

    for name, val in namespace_dict.items():
        if not name.startswith('__'):
            setattr(dummyModule, name, val)
    return unittest.TestLoader().loadTestsFromModule(dummyModule)

def _force_maya_host_process_exit(exit_code, stage=""):
    """End the hosting maya.exe process immediately (best-effort)."""
    def _log(msg):
        try:
            sys.__stdout__.write(msg + "\n")
            sys.__stdout__.flush()
        except Exception:
            pass

    code = int(exit_code) & 0xFFFFFFFF
    if sys.platform == "win32":
        try:
            import ctypes
            k32 = ctypes.windll.kernel32
            handle = k32.GetCurrentProcess()
            ok = k32.TerminateProcess(handle, code)
            if ok:
                _log("MayaHydra: TerminateProcess(GetCurrentProcess) ok {}".format(stage))
            else:
                err = k32.GetLastError()
                _log("MayaHydra: TerminateProcess(GetCurrentProcess) failed {} (err={})".format(
                    stage, err))
                # Try OpenProcess as a fallback.
                PROCESS_TERMINATE = 0x0001
                h2 = k32.OpenProcess(PROCESS_TERMINATE, False, os.getpid())
                if h2:
                    ok2 = k32.TerminateProcess(h2, code)
                    if ok2:
                        _log("MayaHydra: TerminateProcess(OpenProcess) ok {}".format(stage))
                    else:
                        err2 = k32.GetLastError()
                        _log("MayaHydra: TerminateProcess(OpenProcess) failed {} (err={})".format(
                            stage, err2))
                    k32.CloseHandle(h2)
                else:
                    err3 = k32.GetLastError()
                    _log("MayaHydra: OpenProcess(PROCESS_TERMINATE) failed {} (err={})".format(
                        stage, err3))
                # Last resort: ExitProcess (may deadlock on DLL detach).
                _log("MayaHydra: ExitProcess fallback {}".format(stage))
                k32.ExitProcess(code)
        except Exception:
            pass
    os._exit(code)

def runTests(globals_dict, stream=sys.__stderr__,
             verbosity=1, coverage_quit_workaround=False):
    '''
    Run the unittests within the given namespace

    Intended usage:
        import fixturesUtils
        if __name__ == '__main__':
            fixturesUtils.runTests(globals())
    '''
    import maya.cmds as cmds
    suite = loadTestsFromDict(globals_dict)
    runner = unittest.TextTestRunner(stream=stream, verbosity=verbosity)
    coverage_enabled = coverage_quit_workaround and os.environ.get("MAYAHYDRA_CODE_COVERAGE")
    coverage_start = None
    if coverage_enabled:
        coverage_start = time.monotonic()
        sys.__stdout__.write(
            "MayaHydra: coverage timing start (pid={}, t={:.3f})\n".format(
                os.getpid(), coverage_start)
        )
        sys.__stdout__.flush()
    results = runner.run(suite)
    if coverage_enabled and coverage_start is not None:
        sys.__stdout__.write(
            "MayaHydra: coverage timing after tests (dt={:.3f}s)\n".format(
                time.monotonic() - coverage_start)
        )
        sys.__stdout__.flush()
    if results.wasSuccessful():
        exitCode = 0
    else:
        exitCode = 1

    # Coverage builds sometimes hang during Maya shutdown; add diagnostics.
    coverage_dump_traceback_armed = False
    if coverage_enabled:
        # Hard-exit watchdog in case diagnostics or shutdown hang.
        hard_exit_delay = int(os.environ.get("MAYAHYDRA_COVERAGE_HARD_EXIT_DELAY", "5"))
        def _hard_exit_after_delay():
            time.sleep(hard_exit_delay)
            try:
                sys.__stdout__.write(
                    "MayaHydra: coverage hard-exit fired ({}s)\n".format(hard_exit_delay)
                )
                sys.__stdout__.flush()
            except Exception:
                pass
            _force_maya_host_process_exit(exitCode, stage="(hard-exit)")
        threading.Thread(
            target=_hard_exit_after_delay,
            name="MayaHydraCoverageHardExit",
            daemon=True,
        ).start()
        sys.__stdout__.write(
            "MayaHydra: coverage hard-exit scheduled ({}s)\n".format(hard_exit_delay)
        )
        sys.__stdout__.flush()
        try:
            import faulthandler
            faulthandler.dump_traceback_later(120, repeat=True, file=sys.__stderr__)
            coverage_dump_traceback_armed = True
            sys.__stdout__.write("MayaHydra: coverage quit watchdog armed (120s)\n")
        except Exception as e:
            sys.__stdout__.write("MayaHydra: failed to arm quit watchdog: {}\n".format(e))
        try:
            sys.__stdout__.write("MayaHydra: pre-quit diagnostics\n")
            sys.__stdout__.write("  MAYAHYDRA_CODE_COVERAGE={}\n".format(
                os.environ.get("MAYAHYDRA_CODE_COVERAGE")))
            sys.__stdout__.write("  listRenderers={}\n".format(
                cmds.mayaHydra(listRenderers=True)))
            sys.__stdout__.write("  listActiveRenderers={}\n".format(
                cmds.mayaHydra(listActiveRenderers=True)))
        except Exception as e:
            sys.__stdout__.write("MayaHydra: pre-quit diagnostics failed: {}\n".format(e))
        if coverage_start is not None:
            sys.__stdout__.write(
                "MayaHydra: coverage timing pre-quit (dt={:.3f}s)\n".format(
                    time.monotonic() - coverage_start)
            )
        sys.__stdout__.flush()

    # cmds.quit will not flush the streams - make sure we do so!
    # ...flush all of the standard ones just to be sure, as well as the stream
    # given (which probably means it will be flushed twice, but that's fine)
    sys.stdout.flush()
    sys.stderr.flush()
    sys.__stdout__.flush()
    sys.__stderr__.flush()
    stream.flush()

    # Coverage builds may hang during Maya shutdown; force exit with test status.
    if coverage_enabled:
        if coverage_start is not None:
            sys.__stdout__.write(
                "MayaHydra: coverage timing before force-exit (dt={:.3f}s)\n".format(
                    time.monotonic() - coverage_start)
            )
        sys.__stdout__.write(
            "MayaHydra: coverage build forcing exit (exitCode={})\n".format(exitCode)
        )
        sys.__stdout__.flush()
        # Leave the faulthandler watchdog armed: TerminateProcess should
        # return instantly, but if anything stalls we still get a traceback.
        _force_maya_host_process_exit(exitCode, stage="(force-exit)")

    # maya running interactively will absorb much of the output. comment out the
    # following to prevent maya from exiting and open the script editor to look
    # at failures.
    if cmds.about(batch=True):
        # In mayabatch 'quit -abort' does not reliably produce the proper exit
        # code, so use Python instead.
        sys.exit(exitCode)
    else:
        cmds.quit(abort=True, exitCode=exitCode)
