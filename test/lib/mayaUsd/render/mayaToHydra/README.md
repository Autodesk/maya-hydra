# MayaHydra testing

Tests for MayaHydra are based on the [Python unittest framework](https://docs.python.org/3/library/unittest.html) and [Google's GoogleTest framework](https://google.github.io/googletest/), and run in an interactive Maya session.

The tests interact with Maya through its Python commands. MEL scripts/commands can be run by calling `maya.mel.eval("[some MEL script]")`.

There are two types of tests : Python-only tests and Python/C++ tests. GoogleTest is only used in the context of Python/C++ tests. Note that since C++ tests require a Python test to drive them, mentions of C++ tests or testing also imply usage of Python.

Complementary info specific to C++ tests can be found in the [README.md](./cpp/README.md) of the cpp subfolder.

# Running tests

You will first need to build MayaHydra in order to run tests (see [build.md](../../../../../doc/build.md)).

Then, from your build directory (e.g. `.../build/[RelWithDebInfo|Debug]`), run the following command (pick `RelWithDebInfo` or `Debug` according to your build variant) : 

```ctest -C [RelWithDebInfo|Debug] -VV --output-on-failure```

To run a specific test suite, use the `-R` option :

```ctest -C [RelWithDebInfo|Debug] -VV --output-on-failure -R [yourTestFileName]```

For example, to run testVisibility on RelWithDebInfo :

```ctest -C RelWithDebInfo -VV --output-on-failure -R testVisibility```

Note that when running Python tests the Python source is read directly from the source directory.  Therefore, when changing Python test code source, no build needs to be done to update the test, which can be run immediately using ctest.  This is a convenient time saver, as the test development cycle is not edit, build, run, but simply edit and run.  Only if C++ test code is changed is a build then necessary.

### Running tests with MayaUSD

The tests require MayaUSD in order to be run. In order for the test framework to find and load MayaUSD, the `-DMAYAUSD_LOCATION` CMake flag must be set to your MayaUSD installation directory when building MayaHydra. For convenience, you can use the `--mayausd-location` parameter when using the [build.py](../../../../../build.py) script, which will set the CMake flag for you with the given argument. The specified path should point to the directory where your MayaUSD `.mod` file resides. Note that the test framework will not find MayaUSD using the MAYA_MODULE_PATH environment variable.

For example, if your MayaUSD `.mod` file resides in `<some-path>/install/RelWithDebInfo`, you could call build.py as such :

```build.py <some-args> --mayausd-location=<some-path>/install/RelWithDebInfo```


# MayaHydra tests folder structure

The [current folder](./) contains :
- Python test files for the Python-only tests
- Folders for the Python-only test suites (used to store content files, e.g. reference images for comparison tests)

The [cpp subfolder](./cpp) contains : 
- Python test files for the C++ tests
- C++ test files
- Folders for the C++ test suites (used to store content files, e.g. reference images for comparison tests)

The [/test/testUtils](../../../../testUtils/) folder contains :
- Python utility files for testing

# Adding tests

Looking to add a C++ test suite? See the corresponding [README.md](./cpp/README.md) in the cpp subfolder.

To add a new Python-only test suite : 

1. Create a new test[...].py file in the current folder.
2. Create a test class that derives from `mtohUtils.MayaHydraBaseTestCase`.
3. Add the line `_file = __file__` in your class definition.
4. If needed, set `_requiredPlugins` to list any plugins that need to be loaded for your test. MayaUSD does not need to be specified, it will be loaded automatically.
5. Add `test_[...]` methods for each test case in your test suite.
6. Add the following snippet at the bottom of your file :
    ```python
    if __name__ == '__main__':
        fixturesUtils.runTests(globals())
    ```
7. Add the name of your test[...].py file to the list of `TEST_SCRIPT_FILES` in the [current folder's CMakeLists.txt](./CMakeLists.txt)

Some important notes :
- Before each test, a new file will be opened and the renderer will be switched to Hydra. If you need to switch between renderers, you can use the `self.setHdStormRenderer()` and `self.setViewport2Renderer()` methods.
- Some test settings (such as turning color management off, disabling the grid, etc.) are applied by default when opening a scene using `mayaUtils.openNewScene` or `mayaUtils.openTestScene`. The new scene opened before each test is opened using `mayaUtils.openNewScene`, meaning those test settings will automatically be applied before each test. These methods provide the option not to apply those settings if desired, by passing in `useTestSettings=False`. If you want to test a setting that is modified as part of those test settings, you can either re-set it explicitly in the test, and/or load a scene with the desired settings by calling `mayaUtils.openTestScene(<path arguments>, useTestSettings=False)`. In case you want to avoid ever changing the test settings entirely, you can also override the `setUp` method.

# Best practices
- Don't skip tests unless necessary. If a test requires a certain plugin to be loaded, don't skip the test if the plugin fails to load, as this will falsely be reported as a pass. For such cases, prefer setting the `_requiredPlugins` variable in your test class.
- Use relative paths in test data to make sure the tests will work anywhere.
- Prefer storing USD data in text-form .usda instead of binary, for readability and ease of modification should a test need to be tweaked.
- By default, tests will disable color management. However, it tends to be automatically re-enabled in certain conditions; while we have mitigated this by re-disabling it every time we've seen this occur, it is safer to make sure it is explicitly disabled if possible, such as by saving test scenes with color management off (as opposed to saving them with an undefined color management status, or having it enabled).
- If you add tests for color management, do **NOT** use the legacy SynColor system, as it is no longer supported on OSX.

# Image comparison

Image comparison is done using [idiff from OpenImageIO](https://openimageio.readthedocs.io/en/latest/idiff.html).

Some utilities exist to facilitate image comparison tests. Here is the simplest way to get going : 
1. Make sure your test class derives from `mtohUtils.MayaHydraBaseTestCase` (and has the line `_file = __file__`).
2. Create a folder called [TestName]Test (e.g. for testVisibility, create a folder VisibilityTest)
3. Put the images you want to use for comparison in that folder
4. Call `self.assertImagesClose`, `self.assertImagesEqual` and/or `self.assertSnapshotClose` with the file names of the images to compare with.

For the `assert[...]Close` methods, you will need to pass in a fail threshold for individual pixels and a fail percentage for the whole image. Other parameters are also available for you to specify. You can find more information on these parameters on the [idiff documentation page](https://openimageio.readthedocs.io/en/latest/idiff.html).

The `assertSnapshotClose` method takes a snapshot of the current viewport and compares it with the reference image, to ensure the visual result is similar enough.

To create a reference snapshot, an easy way is to first write your test, and then run it once. The output snapshot of the test will then be located in your build folder under `build/[RelWithDebInfo|Debug]/test/lib/mayaUsd/render/mayaToHydra/[testName]`. If the snapshot corresponds to the expected result, you can then copy it over to your `[TestName]Test` folder as mentioned above in step 3. This ensures that the reference image was created with the same playblast settings that will be used when running the tests (e.g. width and height). If this method doesn't suit you, you can use the playblast command directly in Maya yourself, or any other method that fits your needs.

:warning: **Important note** : While there *is* an `assertSnapshotEqual` method, its use is discouraged, as renders can differ very slightly between renderer architectures.

# Adding labels for tests
ctest allows attaching labels to tests to determine whether a test needs to be run or not.
 
For example, you might have some tests that are all related to 'performance', so you label them with 'performance'. Other tests might be about 'security', so you label them 'security'. Later, when you want to run your tests, you can tell CTest to only run the tests with a certain label. Say you only want to run your 'performance' tests. You can tell CTest to do this by using the `-L` option followed by 'performance'. CTest will then only run the tests that have the 'performance' label, and ignore the rest. Similarly, tests can be excluded by passing arguments to `-LE` flag.

To add label in MayaHydra test suite
1. For any particlar unit test of interest in [CMakeLists.txt](./CMakeLists.txt), add the required labels with the following convention:
    `testMyTestFile.py|depOnPlugins:lookdev,mtoa,newPlugin|skipOnPlatform:[osx,win,lin]`
2. Where the label groups follows the unit test name and are separated by `'|'`. You may have multiple label groups.
3. The label groups are of key:values type. They label key can be any meaningful string and has correspondence to the accompanying [json file](../../../../../../maya-hydra/tests-to-run.json)
4. The label values are comma separated lists. The values are added to the test via cmake's `set_tests_properties`

MayaHydra currently is filtering tests based on dependent plugins and platform on which to run them.

To add flags to ctest commandline, modify the corresponding [JSON file](../../../../../../maya-hydra/tests-to-run.json) to indicate which test to run or skip. 
For ex: if you want to run only tests tagged with "lookdevx" then edit the json file like so: `"plugins_to_include": ["lookdevx]`. If the include list is empty, all tests are run by default. Similarly, edit `"plugin_to_exclude"` in the json file to skip test with certain labels.

Note that for platform dependent runs, we only support exclusion mode for a single platform. So if you tag the test `skipOnPlatform:osx`, it skips on MacOS. No corresponding changes to json file is required in this case.

Label mapping
As mentioned earlier in (4), the label key/values in CMakeLists have a correspondence to the values in [JSON file](../../../../../../maya-hydra/tests-to-run.json). For ex:
A line in CMakeLists.txt like so:
`testMyNewTestFile|depOnPlugins:lookdevx,mtoa` would have a corresponding JSON file like so: 
`"plugins_to_include": ["lookdevx","mtoa"]`

`testAnotherTestFile||depOnPlugins:bifrost` would have a corresponding JSON file like so: 
`"plugins_to_exclude": ["bifrost"]`

Similar pattern applies for exclusion/inclusion of tests by filenames except that there is no correpsonding change required in CMakeLists.txt as the pattern matching is by the test name.

Note that `depOnPlugins` key string is only used as cue while adding values in the JSON. The variables in the JSON file and the label values in the CMakeLists.txt are what get used by ctest for label matching.
`"files_to_exclude": ["testStandardSurface"]`

The supported values for platform exclusion are: "osx", "win", "lin".



# Utilities

Utility files are located under [/test/testUtils](../../../../testUtils/). Note that at the time of writing this (2023/08/16), many of the utils files and their contents were inherited from the USD plugin, and are not all used.
