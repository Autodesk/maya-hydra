# Building

## Getting and Building the Code

The simplest way to build the project is by running the supplied **build.py** script. This script builds the project and installs all of the necessary libraries and plug-ins for you. Follow the instructions below to learn how to use the script. 

#### 1. Tools and System Prerequisites

Before building the project, consult the following table to ensure you use the recommended version of compiler, operating system, cmake, etc. 

|        Required       | ![](images/windows.png)   |                            ![](images/mac.png)               |   ![](images/linux.png)     |
|:---------------------:|:-------------------------:|:------------------------------------------------------------:|:---------------------------:|
|    Operating System   |         Windows 10 <br> Windows 11 | High Sierra (10.13)<br>Mojave (10.14)<br>Catalina (10.15)<br>Big Sur (11.2.x)    |      Rocky Linux 8.6 / Linux® Red Hat® Enterprise 8.6 WS             |
|   Compiler Requirement| Maya 2024 (VS 2022) | Maya 2024 (Xcode 13.4 or higher) | Maya 2024 (gcc 11.2.1) |
| CMake Version (min/max) |        3.13...3.17      |                              3.13...3.17                     |           3.13...3.17       |
|         Python        | 3.10.8  |                       3.10.8               |  3.10.8   |
|    Python Packages    | PyYAML, PySide, PyOpenGL, Jinja2        | PyYAML, PySide2, PyOpenGL, Jinja2              | PyYAML, PySide, PyOpenGL, Jinja2 |
|    Build generator    | Visual Studio, Ninja (Recommended)    |  XCode, Ninja (Recommended)                      |    Ninja (Recommended)      |
|    Command processor  | Visual Studio X64 2019 command prompt  |                     bash                |             bash            |
| Supported Maya Version|  2024   |                   2024                    |   2024    |

|        Optional       | ![](images/windows.png)   |                            ![](images/mac.png)               |   ![](images/linux.png)     |

***NOTE:*** Visit the online Maya developer help document under ***Setting up your build environment*** for additional compiler requirements on different platforms.

#### 2. Download and Build Pixar USD 

See Pixar's official github page for instructions on how to build USD: https://github.com/PixarAnimationStudios/USD. Pixar has recently removed support for building Maya USD libraries/plug-ins in their github repository and ```build_usd.py```.

|               |      ![](images/pxr.png)          |        
|:------------: |:---------------:                  |
|  CommitID/Tags | Recommended : [v23.08](https://github.com/PixarAnimationStudios/OpenUSD/releases/tag/v23.08) |
|  CommitID/Tags | For older maya-hydra plugin: [v22.11](https://github.com/PixarAnimationStudios/USD/releases/tag/v22.11) |

For additional information on building Pixar USD, see the ***Additional Build Instruction*** section below.

***NOTE:*** Recommended version of USD for building the latest version of MayaHydra plugin is USD23.08  [v23.08](https://github.com/PixarAnimationStudios/OpenUSD/releases/tag/v23.08). If older version of USD needs to be used then maya-hydra v0.1.x is to be used for build and feature compatibility.

***NOTE:*** Make sure that you don't have an older USD locations in your ```PATH``` and ```PYTHONPATH``` environment settings. ```PATH``` and ```PYTHONPATH``` are automatically adjusted inside the project to point to the correct USD location. See ```cmake/usd.cmake```.

#### 3. Universal Front End (UFE)

The Universal Front End (UFE) is a DCC-agnostic component that allows Maya to browse and edit data in multiple data models. This allows Maya to edit pipeline data such as USD. UFE comes installed as a built-in component with Maya 2019 and later. UFE is developed as a separate binary component, and therefore versioned separately from Maya.

| Ufe Version                | Maya Version                                           | Ufe Docs (external) |
|----------------------------|--------------------------------------------------------|:-------------------:|
| v4.0.0                      | Maya 2024                                                | https://help.autodesk.com/view/MAYAUL/2024/ENU/?guid=MAYA_API_REF_ufe_ref_index_html |
| v4.0.1                      | Maya 2024.1                                               | |

To build the project with UFE support, you will need to use the headers and libraries included in the ***Maya Devkit***:

https://www.autodesk.com/developer-network/platform-technologies/maya

#### 4. Download the source code

Start by cloning the repository:
```
git clone https://github.com/Autodesk/maya-hydra 
cd maya-hydra
```

##### Repository Layout

| Location      | Description                                                                                   |
|-------------  |---------------------------------------------------------------------------------------------  |
| lib/mayaHydra/mayaPlugin | Contains Maya plugin definition and render override registration  |
| lib/mayaHydra/hydraExtensions | Contains extensions to and mechanism needed to interface with hydra classes |
| lib/mayaHydra/ufeExtensions | Contains extensions to translate paths between UFE, USD SdfPath and Maya DAGPath |

#### 5. How To Use build.py Script

##### Arguments

There are four arguments that must be passed to the script: 

| Flags                 | Description                                                                           |
|--------------------   |-------------------------------------------------------------------------------------- |
|   --maya-location     | directory where Maya is installed.                                                    |
|  --pxrusd-location    | directory where Pixar USD Core is installed.                                          |
|  --devkit-location    | directory where Maya devkit is installed.                                             |
| workspace_location    | directory where the project use as a workspace to build and install plugin/libraries  |

```
Linux:
➜ maya-hydra python build.py --maya-location /usr/autodesk/maya2024 --pxrusd-location /usr/local/USD-Release --devkit-location /usr/local/devkitBase /usr/local/workspace

MacOSX:
➜ maya-hydra python build.py --maya-location /Applications/Autodesk/maya2024 --pxrusd-location /opt/local/USD-Release --devkit-location /opt/local/devkitBase /opt/local/workspace

Windows:
c:\maya-hydra> python build.py --maya-location "C:\Program Files\Autodesk\Maya2024" --pxrusd-location C:\USD-Release --devkit-location C:\devkitBase C:\workspace
```

##### Build Arguments

| Flag                  | Description                                                                           |
|--------------------   |---------------------------------------------------------------------------------------|
|   --build-args        | comma-separated list of cmake variables can be also passed to build system.           |

```
--build-args="-DBUILD_TESTS=OFF"
```

##### CMake Options

Name                        | Description                                                | Default
---                         | ---                                                        | ---
BUILD_TESTS                 | builds all unit tests.                                     | ON
BUILD_STRICT_MODE           | enforces all warnings as errors.                           | ON
BUILD_SHARED_LIBS			| build libraries as shared or static.						 | ON

##### Stages

| Flag                 | Description                                                                           |
|--------------------  |--------------------------------------------------------------------------------------------------- |
|   --stages           | comma-separated list of stages can also be passed to the build system. By default "clean, configure, build, install" stages are executed if this argument is not set. |

| Options       | Description                                                                                   |
|-----------    |---------------------------------------------------                                            |
| clean         | clean build                                                                                   |
| configure     | call this stage every time a cmake file is modified                                           |
| build         | builds the project                                                                            |
| install       | installs all the necessary plug-ins and libraries                                             |
| test          | runs all unit tests                                                              |
| package       | bundles up all the installation files as a zip file inside the package directory              |

```
Examples:
--stages=configure,build,install
--stages=test
```
***NOTE:*** All the flags can be followed by either ```space``` or ```=```

##### CMake Generator

It is up to the user to select the CMake Generator of choice, but we encourage the use of the Ninja generator. To use the Ninja Generator, you need to first install the Ninja binary from https://ninja-build.org/

You then need to set the generator to ```Ninja``` and the ```CMAKE_MAKE_PROGRAM``` variable to the Ninja binary you downloaded.
```
python build.py --generator Ninja --build-args=-DCMAKE_MAKE_PROGRAM='path to ninja binary'
```
##### Build and Install locations

By default, the build and install directories are created inside the **workspace** directory. However, you can change these locations by setting the ```--build-location``` and ```--install-location``` flags. 

##### Build Log

By default the build log is written into ```build_log.txt``` inside the build directory. If you want to redirect the output stream to the console instead,
you can pass ```--redirect-outstream-file``` and set it to false.

##### Additional flags and options

Run the script with the ```--help``` parameter to see all the possible flags and short descriptions.

#### 6. How To Run Unit Tests

Unit tests can be run by setting ```--stages=test``` or by simply calling `ctest` directly from the build directory.

# Additional Build Instruction

##### Python:

It is important to use the Python version shipped with Maya and not the system version when building USD on MacOS. Note that this is primarily an issue on MacOS, where Maya's version of Python is likely to conflict with the version provided by the system. 

To build USD and the Maya plug-ins on MacOS for Maya (2024), run:
```
/Applications/Autodesk/maya2024/Maya.app/Contents/bin/mayapy build_usd.py ~/Desktop/BUILD
```
By default, ``usdview`` is built which has a dependency on PyOpenGL. Since the Python version of Maya doesn't ship with PyOpenGL you will be prompted with the following error message:
```
PyOpenGL is not installed. If you have pip installed, run "pip install PyOpenGL" to install it, then re-run this script.
If PyOpenGL is already installed, you may need to update your ```PYTHONPATH``` to indicate where it is located.
```
The easiest way to bypass this error is by setting ```PYTHONPATH``` to point at your system python or third-party python package manager that has PyOpenGL already installed.
e.g
```
export PYTHONPATH=$PYTHONPATH:Library/Frameworks/Python.framework/Versions/3.10/lib/python3.10/site-packages
```
Use `pip list` to see the list of installed packages with your python's package manager.

e.g
```
➜ pip list
Package    Version
---------- --------
Jinja2     3.1.2
MarkupSafe 2.1.1
pip        22.2.1
PyOpenGL   3.1.6
PySide2    5.15.2.1
PyYAML     6.0
setuptools 63.2.0
shiboken2  5.15.2.1
```

##### Dependencies on Linux DSOs when running tests

Normally either runpath or rpath are used on some DSOs in this library to specify explicit on other libraries (such as USD itself)

If for some reason you don't want to use either of these options, and switch them off with:
```
CMAKE_SKIP_RPATH=TRUE
```
To allow your tests to run, you can inject LD_LIBRARY_PATH into any of the mayaHydra_add_test calls by setting the ADDITIONAL_LD_LIBRARY_PATH cmake variable to $ENV{LD_LIBRARY_PATH} or similar.

There is a related ADDITIONAL_PXR_PLUGINPATH_NAME cmake var which can be used if schemas are installed in a non-standard location

# How to Load Plug-ins in Maya 

The provided module files (*.mod) facilitates setting various environment variables for plugins and libraries. After the project is successfully built, ```mayaHydra.mod``` are installed inside the install directory. In order for Maya to discover these mod files, ```MAYA_MODULE_PATH``` environment variable needs to be set to point to the location where the mod files are installed.
Examples:
```
set MAYA_MODULE_PATH=C:\workspace\install\RelWithDebInfo
export MAYA_MODULE_PATH=/usr/local/workspace/install/RelWithDebInfo
```
Once MAYA_MODULE_PATH is set, run maya and go to ```Windows -> Setting/Preferences -> Plug-in Manager``` to load the plugins.
