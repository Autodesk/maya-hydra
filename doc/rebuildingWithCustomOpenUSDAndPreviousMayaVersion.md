# Building with the latest version of OpenUSD and with a previous version of Maya such as 2024

Let's take as an example rebuilding MayaHydra in <b>Maya 2024</b> and <b>OpenUSD 24.11</b> under Windows.<br>

Please be aware that <b>Maya 2024 is not officially supported </b>since we have added a few features to Maya API in newer versions of Maya.
Known limitations when using MayaHydra in Maya 2024 :
- The cull mode from Maya geometries does not work.
- Isolate select does not function correctly with USD stages.
- using LookdevX to author new MaterialX materials won't work, but loading a usd stage with MaterialX materials saved in the stage will work.

With Maya 2024 the OpenUSD version used by default was 22.11 which was compiled with Python 3.10.8.

## Rebuilding OpenUSD

If OpenUSD was not built with the same version of Python as the one used in your Maya version, you need to rebuild it with the suitable Python :<br>

|        Maya version       | Python version   |
|:-------------------------:|:----------------:|
| 2024                 | 3.10.8           |
| 2025                 | 3.11.4           |
| PR                   | 3.11.9           |

So for Maya 2024, we need to rebuild OpenUSD with Python 3.10.8.<br>
To do so, you need to clone the OpenUSD repository and checkout the version used by MayaUsd and MayaHydra which is [v24.11-MayaUsd-Public](https://github.com/autodesk-forks/USD/tree/v24.11-MayaUsd-Public).<br>
As an example here is a Windows batch file to rebuild OpenUSD with Python 3.10.8 which is provided by mayapy.exe in the Maya/bin folder:<br><br>
First, to rebuild both OpenUSD and MayaUsd, you may need to install the following Python modules for this version of Python: <br>
PyOpenGL==3.1.0, PySide6, jinja2 and pybind11.<br>
We are assuming that Maya 2024 is installed in its default folder such as C:\Program Files\Autodesk\Maya2024.<br>
```
"C:\Program Files\Autodesk\Maya2024\bin\mayapy.exe" -m pip install PyOpenGL==3.1.0
"C:\Program Files\Autodesk\Maya2024\bin\mayapy.exe" -m pip install PySide6
"C:\Program Files\Autodesk\Maya2024\bin\mayapy.exe" -m pip install jinja2
"C:\Program Files\Autodesk\Maya2024\bin\mayapy.exe" -m pip install pybind11
```
To rebuild OpenUSD see the following Windows batch file as an example :
<br>
```
rem rebuild OpenUSD with Python 3.10.8 for maya 2024

rem The following modules may need to be installed in mayapy.exe: PyOpenGL==3.1.0, PySide6 and pybind11.

set openusd_maya_location=C:/Program^ Files/Autodesk/Maya2024
set openusd_python_include=%openusd_maya_location%/include/Python310/Python
set openusd_python_exe=%openusd_maya_location%/bin/mayapy.exe
set openusd_python_lib=%openusd_maya_location%/lib/python310.lib

rem our destination folder for the build
set destination_folder="D:/USD-24.11-Python3.10.8"

rem rebuilding OpenUSD with Python 3.10.8 in release with debug info configuration, if you want to use release only, please change the build-variant
"%openusd_python_exe%" ".\build_scripts\build_usd.py" "%destination_folder%" ^
	--build-variant=relwithdebuginfo ^
	--build-python-info "%openusd_python_exe%" "%openusd_python_include%" "%openusd_python_lib%" 3.10.8 ^
	--onetbb ^
	--alembic ^
	--openimageio ^
	--no-hdf5 ^
	--no-embree ^
	--materialx ^
	--no-debug-python ^
	--no-tests
```

## Rebuilding MayaUsd

Since MayaHydra is dependent on MayaUsd (through the MayaUsdAPI module), you need to rebuild MayaUsd with the same version of Python and with OpenUSD previously rebuilt.<br>
To rebuild MayaUsd, you need to clone the MayaUsd repository (see [build.md](./build.md) for more info) and rebuild for Windows with (adjust it to your custom folders) :<br>
```
rem Save the current PATH as we are going to modify it
set original_path=%PATH%

rem OpenUSD 24.11 rebuilt with Python 3.10.8
set mayausd_pixar_usd_location=D:/USD-24.11-Python3.10.8

rem download and install the Maya 2024 devkit in a folder
set mayausd_maya_devkit_location=D:/Autodesk_Maya_2024_DEVKIT_Windows/devkitBase

rem Maya 2024 location
set mayausd_maya_location=C:/Program^ Files/Autodesk/Maya2024
set mayausd_python_include=%mayausd_maya_location%/include/Python310/Python
set mayausd_python_exe=%mayausd_maya_location%/bin/mayapy.exe
set mayausd_python_lib=%mayausd_maya_location%/lib/python310.lib

rem Remove other Python installations from the PATH to avoid conflict 
rem As I had Python 3.11 installed. It would also be possible to use a Python virtual environment: https://docs.python.org/3/library/venv.html
set PATH=C:\Program Files\Autodesk\Maya2024\bin;%original_path%
set PATH=%PATH:C:\Users\%username%\AppData\Local\Programs\Python\Python311\Scripts;=%
set PATH=%PATH:C:\Users\%username%\AppData\Local\Programs\Python\Python311;=%
set PATH=%PATH:C:\Users\%username%\AppData\Local\Programs\Python\Python311\=%

rem Add OpenUSD bin and lib path to the Windows PATH
set PATH=%mayausd_pixar_usd_location%\bin;%mayausd_pixar_usd_location%\lib;%PATH%

rem Set necessary environment variables for the build process
set PYTHONPATH=C:\Program Files\Autodesk\Maya2024\Python;C:\Program Files\Autodesk\Maya2024\Lib;%mayausd_pixar_usd_location%\lib\python;

set mayausd_destination=D:/maya-usd-Python3.10.8

rem change this to your local version of Qt
set mayausd_qtlocation=D:/GIT/maya/artifactory/Windows/qt/6.5.3/60c0d89
set mayausd_qtversion=6.5.3

rem Output diagnostic information
echo PATH=%PATH%
echo PYTHONPATH=%PYTHONPATH%

rem Use the mayapy.exe directly for the build process
"%mayausd_python_exe%" build.py "%mayausd_destination%_" ^
	-v=3 ^
	--stages=clean,configure,build,install ^
	--pxrusd-location=%mayausd_pixar_usd_location% ^
	--build-relwithdebug  ^
	--maya-location="%mayausd_maya_location%" ^
	--generator="Visual Studio 17 2022" ^
	--devkit-location="%mayausd_maya_devkit_location%" ^
	--materialx ^
	--build-args="-DPYTHON_INCLUDE_DIR=\"%mayausd_python_include%\",-DPython_EXECUTABLE=\"%mayausd_python_exe%\",-DPython3_VERSION=3.10.8,-DPYTHON_LIBRARIES=\"%mayausd_python_lib%\",-DBUILD_WITH_PYTHON_3=ON,-DBUILD_WITH_PYTHON_3_VERSION=3.10,-DBUILD_TESTS=ON,-DBUILD_PXR_PLUGIN=OFF,-DBUILD_AL_PLUGIN=OFF,-DQT_LOCATION=\"%mayausd_qtlocation%\",-DQT_VERSION=%mayausd_qtversion%,-DPython3_ROOT_DIR=\"%mayausd_maya_location%\",-DPython3_FIND_STRATEGY=LOCATION,-DPython3_INCLUDE_DIR=\"%mayausd_python_include%\",-DPython3_LIBRARY=\"%mayausd_python_lib%\""

rem Restore the original PATH
set PATH=%original_path%

rem Unset the Python environment variables
set PYTHONPATH=
```

Note : <br>
You may have the following issue when rebuilding MayaUsd: https://github.com/PixarAnimationStudios/OpenUSD/issues/3310 <br>
In this case remove "OpenGL::GL" from the cmake file :<br>
D:/USD-24.11-Python3.10.8/cmake/pxrTargets.cmake <br>
When rebuilding MayaUsd, manually add the link to "OpenGL32.lib" to the MayaUsd project under Visual Studio in order to link correctly.


## Rebuilding MayaHydra

OpenUSD and MayaUsd are now rebuilt with the same version of Python as Maya 2024.  MayaHydra can now be built.<br>
Which under Windows can be made through the follow batch file (adjust it to your custom folders):<br>
```
rem global options to build MayaHydra

set mayahydra_verboseLevel=3

set maya_hydra_python_version=3.10.8

rem MayaUsd
set mayahydra_mayausdlocation=D:/maya-usd-Python3.10.8

rem OpenUSD 24.11 rebuilt with Python 3.10.8
set mayahydra_pxrusdlocation=D:/USD-24.11-Python3.10.8

rem this is for release with debug info, change it to release if you want to build in release only
set mayahydra_buildconfig=relwithdebug

set mayahydra_mayalocation=C:/Program^ Files/Autodesk/Maya2024
set mayahydra_pythonincludedir=%mayahydra_mayalocation%/include/Python310/Python
set mayahydra_pythonexecutable=%mayahydra_mayalocation%/bin/mayapy.exe
set mayahydra_pythonlib=%mayahydra_mayalocation%/lib/python310.lib

set mayahydra_generator="Visual Studio 17 2022"
rem set mayahydra_generator="Ninja" you could use ninja if it is available on your system instead of VS2022

set mayahydra_mayadevkitlocation=D:/Autodesk_Maya_2024_DEVKIT_Windows/devkitBase
set mayahydra_main_pythonversion=3.10

rem change this to your local version of Qt
set mayahydra_qtlocation=D:/GIT/maya/artifactory/Windows/qt/6.5.3/60c0d89
set mayahydra_qtversion=6.5.3

rem Save the current PATH as we are going to modify it
set original_path=%PATH%

rem Remove other Python installations from the PATH to avoid conflict 
rem As I had Python 3.11 installed. It would also be possible to use a Python virtual environment: https://docs.python.org/3/library/venv.html
set PATH=C:\Program Files\Autodesk\Maya2024\bin;%original_path%
set PATH=%PATH:C:\Users\%username%\AppData\Local\Programs\Python\Python311\Scripts;=%
set PATH=%PATH:C:\Users\%username%\AppData\Local\Programs\Python\Python311;=%
set PATH=%PATH:C:\Users\%username%\AppData\Local\Programs\Python\Python311\=%

rem Add USD library path to the PATH
set PATH=%mayahydra_pxrusdlocation%\bin;%mayahydra_pxrusdlocation%\lib;%PATH%

rem Set necessary environment variables for the build process
set PYTHONPATH=C:\Program Files\Autodesk\Maya2024\Python;C:\Program Files\Autodesk\Maya2024\Lib;%mayahydra_pxrusdlocation%\lib\python;

rem Output diagnostic information
echo PATH=%PATH%
echo PYTHONPATH=%PYTHONPATH%

"%mayahydra_pythonexecutable%" build.py "D:/maya-hydra-build" ^
	-v=%mayahydra_verboseLevel% ^
	--mayausd-location=%mayahydra_mayausdlocation% ^
	--stages=clean,configure,build,install ^
	--pxrusd-location=%mayahydra_pxrusdlocation% ^
	--build-%mayahydra_buildconfig% ^
	--maya-location="%mayahydra_mayalocation%" ^
	--generator=%mayahydra_generator% ^
	--devkit-location=%mayahydra_mayadevkitlocation% ^
	--build-args="-DPYTHON_INCLUDE_DIR=\"%mayahydra_pythonincludedir%\",-DPython_EXECUTABLE=\"%mayahydra_pythonexecutable%\",-DPYTHON_LIBRARIES=\"%mayahydra_pythonlib%\",-DBUILD_WITH_PYTHON_3=ON,-DBUILD_WITH_PYTHON_3_VERSION=%mayahydra_main_pythonversion%,-DBUILD_TESTS=ON,-DBUILD_PXR_PLUGIN=OFF,-DBUILD_AL_PLUGIN=OFF,-DQT_LOCATION=\"%mayahydra_qtlocation%\",-DQT_VERSION=%mayahydra_qtversion%,-DPython3_VERSION=%maya_hydra_python_version% -DPython3_ROOT_DIR=\"%mayahydra_mayalocation%\",-DPython3_FIND_STRATEGY=LOCATION,-DPython3_INCLUDE_DIR=\"%mayahydra_pythonincludedir%\",-DPython3_LIBRARY=\"%mayahydra_pythonlib%\""
	

rem Restore the original PATH
set PATH=%original_path%

rem Unset the Python environment variables
set PYTHONPATH=

```

