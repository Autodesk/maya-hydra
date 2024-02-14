# Code Coverage For Windows

Maya-hydra has support for obtaining code coverage on Windows platforms.

The support uses two main tools: Clang for the compilation of maya-hydra and the LLVM toolset for the parsing of code coverage
information and the generation of a code coverage report.

## Prerequisites

To install Clang and the LLVM toolset, you can install an optional module with Visual Studio.  Refer to these instructions: [Install Clang and LLVM Toolset](https://learn.microsoft.com/en-us/cpp/build/clang-support-msbuild?view=msvc-170)

## Documentation References
- [llvm-profdata and llvm-cov show](https://llvm.org/docs/CommandGuide/llvm-cov.html)
- [Compiling clang with coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html)

## Building the Coverage Variant

The maya-hydra build has a Coverage variant that can be used with the following stages: clean,build,install,test (note that the test stage depends on the install stage).  Clang is used with code coverage instrumentation flags enabled (-fprofile-instr-generate -fcoverage-mapping) so that when tests are run after a successful install stage, code coverage data files will be generated.

To build the coverage variant you can run:

```
python build.py --generator=Ninja --stages clean,configure,build,install --maya-location <maya_location> --build-coverage --pxrusd-location <pxrusd_location> --devkit-location <devkit_location> --build-args="-DPYTHON_INCLUDE_DIR=<python_include_dir>,-DPython_EXECUTABLE=<python_executable>,-DPYTHON_LIBRARIES=<python_libraries>,-DBUILD_WITH_PYTHON_3=ON,-DBUILD_WITH_PYTHON_3_VERSION=<version_of_python>,-DQT_VERSION=6.5.0,-DCMAKE_WANT_MATERIALX_BUILD=ON,-DCMAKE_PREFIX_PATH=<cmake_prefix_path>" <installation_location>
```

The --build-coverage flag indicates that the variant to be built is the Coverage variant. 

## Running Tests and Getting Raw Coverage Information

To run tests and generate code coverage information using the Coverage build, run:

```
python build.py --generator=Ninja --stages test --maya-location <maya_location> --build-coverage --pxrusd-location <pxrusd_location> --devkit-location <devkit_location> --build-args="-DPYTHON_INCLUDE_DIR=<python_include_dir>,-DPython_EXECUTABLE=<python_executable>,-DPYTHON_LIBRARIES=<python_libraries>,-DBUILD_WITH_PYTHON_3=ON,-DBUILD_WITH_PYTHON_3_VERSION=<version_of_python>,-DQT_VERSION=6.5.0,-DCMAKE_WANT_MATERIALX_BUILD=ON,-DCMAKE_PREFIX_PATH=<cmake_prefix_path>" <installation_location>
```

After running tests, there will be raw coverage information files generated that are have a file extension of .profraw. 

## Parsing Coverage Information and Report

To parse the coverage information, two tools from the LLVM toolset are used: llvm-profdata and llvm-cov show. 
llvm-profdata parses and merges all of the raw coverage information files into a single file that has a file extension of .profdata.
llvm-cov show uses this created file to generate an HTML report of the coverage of the maya-hydra plugin. 

There is no build stage in maya-hydra that can automatically run this step of collecting coverage information and creating a report.  To complete this step manually
here are the instructions.

To run llvm-profdata and parse all of the coverage information into a single file, run:

```
llvm-profdata merge -sparse -o '<profdata_file_name>' '<profraw_file_location(s)' 
```

Note:
- Multiple profraw file locations can be specified, each of which should be separated by a space after the profraw file name
- The profdata file name must end with the file extension .profdata

To run llvm-cov show and to generate an HTML report containing code coverage information, run:

```
llvm-cov show -instr-profile=<profdata_file_location> <mayaHydraLib.dll_location> -object=mayaHydra.mll_location -object=flowViewport.dll_location -show-branches=count -show-regions --ignore-filename-regex='artifactory\\.*' -format=html -output-dir=<output_dir_name>'
```

Note:
- -instr-profile refers to the previously generated profdata file, which contains all code coverage information
- --ignore-filename-regex removes any files starting with artifactory from the report

## Viewing Results

Go to the output directory from the llvm-cov show command and open the index.html file. Any lines that are uncovered are red. Lines that are covered have no highlighting around them.


