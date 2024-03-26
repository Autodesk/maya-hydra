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
- When opening a Maya test scene (using `mayaUtils.openTestScene()`), color management will be disabled immediately afterwards, even if the scene had it enabled. If your test is explicitly testing for color management, you will need to re-enable it manually (for example using `maya.cmds.colorManagementPrefs(edit=True, cmEnabled=True)`).

# Best practices
- Don't skip tests unless necessary. If a test requires a certain plugin to be loaded, don't skip the test if the plugin fails to load, as this will falsely be reported as a pass. For such cases, prefer setting the `_requiredPlugins` variable in your test class.
- Use relative paths in test data to make sure the tests will work anywhere.
- Prefer storing USD data in text-form .usda instead of binary, for readability and ease of modification should a test need to be tweaked.
- By default, tests will disable color management. However, it tends to be automatically re-enabled in certain conditions; while we have mitigated this by re-disabling it every time we've seen this occur, it is safer to make sure it is explicitly disabled if possible, such as by saving tests scenes with color management off (as opposed to saving them with an undefined color management status).
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

# Utilities

Utility files are located under [/test/testUtils](../../../../testUtils/). Note that at the time of writing this (2023/08/16), many of the utils files and their contents were inherited from the USD plugin, and are not all used.
