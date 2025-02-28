# Code Coverage For Windows

## Table of contents

- [Introduction](#introduction)
- [Prerequisites](#prerequisites)
- [Documentation References](#documentation-references)
- [Building the Coverage Variant](#building-the-coverage-variant)
- [Running Tests and Getting Raw Coverage Information](#running-tests-and-getting-raw-coverage-information)
- [Parsing Coverage Information and Generating a Report](#parsing-coverage-information-and-generating-a-report)
- [Viewing Results](#viewing-results)

## Introduction

maya-hydra has support for obtaining code coverage on Windows platforms.

The support uses two main tools: Clang for the compilation of maya-hydra and the LLVM toolset for the parsing of code coverage information and the generation of a code coverage report.

## Prerequisites

To install Clang and the LLVM toolset, you can install an optional module with Visual Studio. Refer to these instructions: [Install Clang and LLVM Toolset](https://learn.microsoft.com/en-us/cpp/build/clang-support-msbuild?view=msvc-170)

> Note: On Windows, all commands must be executed in a `x64 Native Tools Command Prompt for VS 2022` command line

## Documentation References
- [llvm-profdata and llvm-cov show](https://llvm.org/docs/CommandGuide/llvm-cov.html)
- [Compiling clang with coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html)

## Building the Coverage Variant

The maya-hydra build has a Coverage variant that can be used with the following stages: `clean,build,install,test` (note that the `test` stage depends on the `install` stage). Clang is used with code coverage instrumentation flags enabled (`-fprofile-instr-generate -fcoverage-mapping`) so that when tests are run after a successful `install` stage, code coverage data files will be generated. Refer to the [build documentation](./build.md) for more details.

Here is the command to build the Coverage variant:

```
python build.py
    --build-coverage ^
    --generator=Ninja ^
    --stages=clean,configure,build,install ^
    --maya-location <maya_location> ^
    --mayausd-location <mayausd_location> ^
    --pxrusd-location <pxrusd_location> ^
    --devkit-location <devkit_location> ^
    --build-args="-DPYTHON_INCLUDE_DIR=<python_include_dir>,-DPython_EXECUTABLE=<python_executable>,-DPYTHON_LIBRARIES=<python_libraries>,-DCMAKE_WANT_MATERIALX_BUILD=ON,-DCMAKE_PREFIX_PATH=<cmake_prefix_path>" ^
    <workspace_location>
```
> Note: Some tests might also require `--mtoa-location` or `--lookdevx-location` to succeed.

The `--build-coverage` flag indicates that the variant to be built is the Coverage variant.

At time of writing (February 26th, 2024), only the Ninja code generator is supported.  In particular, the Visual Studio generator is known not to output code coverage data.

## Running Tests and Getting Raw Coverage Information

To run tests and generate code coverage information using the Coverage build, run the same `build.py` command as in [the previous section](#building-the-coverage-variant), but replace `--stages=clean,configure,build,install` with `--stages=test`.

After running tests, the raw coverage information files will be generated in `<workspace_location>\build\Coverage\test\lib\mayaUsd\render\mayaToHydra\<test_subfolders>`. These files have a `.profraw` file extension.

## Parsing Coverage Information and Generating a Report

To parse the coverage information, two tools from the LLVM toolset are used: `llvm-profdata merge` and `llvm-cov show`.
- `llvm-profdata merge`: Parses and merges all of the raw coverage information files (`.profraw`) into a single file with a `.profdata` extension.
- `llvm-cov show`: Generates an HTML report of the coverage of the maya-hydra plugin using the generated`.profdata` file.

There is no build stage in maya-hydra that can automatically run this step of collecting coverage information and creating a report. Here are the steps to manually generate a coverage report:

1. Generate a list of all the generated `.profraw` files. Run this command from `<workspace_location>\build`:
```
dir /s /b *.profraw > "path\to\profraw_list.txt"
```
`profraw_list.txt` is a text file containing one path to a `.profraw` file per line.

2. Run `llvm-profdata` to combine all the coverage information into a single `.profdata` file using the file list from the previous step:

```
llvm-profdata merge -sparse -o "path\to\profdata_file.profdata" --input-files="path\to\profraw_list.txt"
```
> Note: The profdata file name must end with the file extension `.profdata`

3. Run `llvm-cov` to generate an HTML report containing code coverage information from the `.profdata` file:

```
llvm-cov show ^
    -instr-profile="path\to\profdata_file.profdata" ^
    "<workspace_dir>\install\Coverage\lib\mayaHydraLib.dll" ^
    -object="<workspace_dir>\install\Coverage\lib\maya\mayaHydra.mll" ^
    -object="<workspace_dir>\install\Coverage\lib\flowViewport.dll" ^
    -show-branches=count ^
    -show-regions ^
    -ignore-filename-regex="artifactory\\.*" ^
    -format=html ^
    -output-dir=<output_dir_name>
```
Notes:
- `-instr-profile` refers to the previously generated profdata file
- `--ignore-filename-regex='artifactory\\.*'` removes any files starting with "artifactory" from the report

## Viewing Results

Go to the output directory from the `llvm-cov show` command and open the `index.html` file in a web browser. When browsing individual source files, red highlights indicate lines or branches that were not covered in the tests.
