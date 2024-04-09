# Changelog

[v0.6.1] - 2024-04-05

**Build:**
* Create a unit test for translate, rotate, scale [#51](https://github.com/Autodesk/maya-hydra/pull/51)
* Clean test files and separate interactive and non interactive tests [#50](https://github.com/Autodesk/maya-hydra/pull/50)
* Add a test for Usd stages variants [#46](https://github.com/Autodesk/maya-hydra/pull/46)
* Fix image diff test and skip failing test on OSX [#45](https://github.com/Autodesk/maya-hydra/pull/45)
* Add unit test for maya lights [#52](https://github.com/Autodesk/maya-hydra/pull/52)
* Adjust image diff threshold for testMayaLights [#55](https://github.com/Autodesk/maya-hydra/pull/55)
* Add test for usd lights [#53](https://github.com/Autodesk/maya-hydra/pull/53)
* Add test to cover arnold lights [#56](https://github.com/Autodesk/maya-hydra/pull/56)
* Image-based NURBS unit test [#54](https://github.com/Autodesk/maya-hydra/pull/54)
* Add test for standard surface [#59](https://github.com/Autodesk/maya-hydra/pull/59)
* Maya is no longer using a Preview Release number [#63](https://github.com/Autodesk/maya-hydra/pull/63)
* Image-based polygon primitives tests [#57](https://github.com/Autodesk/maya-hydra/pull/57)
* Add test for look through camera&lights [#73](https://github.com/Autodesk/maya-hydra/pull/73)
* Add Unit Test for Maya Display Modes [#74](https://github.com/Autodesk/maya-hydra/pull/74)
* Curve tools unit tests [#60](https://github.com/Autodesk/maya-hydra/pull/60)
* Add Unit Test for Isolate Select for Maya data [#78](https://github.com/Autodesk/maya-hydra/pull/78)
* Add Unit Test for Maya Shading Modes [#77](https://github.com/Autodesk/maya-hydra/pull/77)
* Add test for template and reference [#76](https://github.com/Autodesk/maya-hydra/pull/76)
* Add Unit Test for Maya Display Layers [#83](https://github.com/Autodesk/maya-hydra/pull/83)
* Add test for cover display & seleciton for object template [#81](https://github.com/Autodesk/maya-hydra/pull/81)
* Add LookdevX support for test framework [#87](https://github.com/Autodesk/maya-hydra/pull/87)
* Add test for payload & reference [#89](https://github.com/Autodesk/maya-hydra/pull/89)
* Expose option to enable/disable MSAA for Storm renderer [#95](https://github.com/Autodesk/maya-hydra/pull/95)
* Investigate why the tumbling performance of USD scene is slower than corresponding Maya Scene under Hydra Storm [#91](https://github.com/Autodesk/maya-hydra/pull/91)
* Adjust image diff fail percent for shading modes test [#99](https://github.com/Autodesk/maya-hydra/pull/99)
* Add picking test + Link C++ tests with Qt for UI/viewport interaction [#94](https://github.com/Autodesk/maya-hydra/pull/94)
* Fix MtoA & LookdevX loading in tests + Related test improvements [#108](https://github.com/Autodesk/maya-hydra/pull/108)
* Fix color management handling in tests [#110](https://github.com/Autodesk/maya-hydra/pull/110)
* Create unit test to verify UVs/UDIMs are rendered accordingly with Maya data [#111](https://github.com/Autodesk/maya-hydra/pull/111)
* Background color and grid tests [#112](https://github.com/Autodesk/maya-hydra/pull/112)
* Fix coverage crashes in newly-added filtering scene indices [#114](https://github.com/Autodesk/maya-hydra/pull/114)

**Features:**
* Add mayaHydra plugin commands to query CPU/GPU memory usage [#102](https://github.com/Autodesk/maya-hydra/pull/102)
* Implement basics of bounding box display style [#103](https://github.com/Autodesk/maya-hydra/pull/103)
* Point instance picking support (no selection highlighting) [#105](https://github.com/Autodesk/maya-hydra/pull/105)
* Native instance picking [#107](https://github.com/Autodesk/maya-hydra/pull/107)
* Expose complexity settings in globalRenderSettings [#106](https://github.com/Autodesk/maya-hydra/pull/106)
* USD pick with kind support, with test [#115](https://github.com/Autodesk/maya-hydra/pull/115)

**Flow Viewport Toolkit:**
* Use RootOverridesSceneIndex to simplify data producer scene indices [#64](https://github.com/Autodesk/maya-hydra/pull/64)
* Fix Mayausd.mod file not being present in downloaded build [#68](https://github.com/Autodesk/maya-hydra/pull/68)
* Add the FootPrint node as an Hydra example [#75](https://github.com/Autodesk/maya-hydra/pull/75)
* Fix the crash when mayaHydra is not loaded and rename classes [#98](https://github.com/Autodesk/maya-hydra/pull/98)
* Add a flow viewport toolkit API .md file [#97](https://github.com/Autodesk/maya-hydra/pull/97)

**Miscellaneous:**
* Move mayaUsd code for managing stages inside mayaHydra [#38](https://github.com/Autodesk/maya-hydra/pull/38)
* Add code coverage build support [#65](https://github.com/Autodesk/maya-hydra/pull/65)
* Add mtoa download support in maya hydra [#71](https://github.com/Autodesk/maya-hydra/pull/71)
* Maya MaterialX material bindings on Maya geometry [#72](https://github.com/Autodesk/maya-hydra/pull/72)
* Updating changelog for v0.6.0 (#49) [#80](https://github.com/Autodesk/maya-hydra/pull/80)
* Made proxy shape scene index prefixes unique, and support rename / reparent [#84](https://github.com/Autodesk/maya-hydra/pull/84)
* Update maya-hydra-preflight-launcher.yml [#67](https://github.com/Autodesk/maya-hydra/pull/67)
* Update maya-hydra-new-issues.yml [#66](https://github.com/Autodesk/maya-hydra/pull/66)
* Try to fix rpath issue [#96](https://github.com/Autodesk/maya-hydra/pull/96)
* Remove SceneDelegate code path [#100](https://github.com/Autodesk/maya-hydra/pull/100)
* Remove Maya Hydra Scene Producer wrapper [#101](https://github.com/Autodesk/maya-hydra/pull/101)

**Bugfix:**
* Filtering scene index example can have prims get unfiltered at the wrong location [#42](https://github.com/Autodesk/maya-hydra/pull/42)
* Fix code coverage build issue [#104](https://github.com/Autodesk/maya-hydra/pull/104)
* Avoid Maya Hydra registration for mayaUsdPlugin if not loaded [#109](https://github.com/Autodesk/maya-hydra/pull/109)
* Fix reload of a scene with a Usd stage [#113](https://github.com/Autodesk/maya-hydra/pull/113)

**Documentation:**
* Added code coverage documentation [#70](https://github.com/Autodesk/maya-hydra/pull/70)
* Readme.md update [#82](https://github.com/Autodesk/maya-hydra/pull/82)
* Minor code coverage documentation fixes [#86](https://github.com/Autodesk/maya-hydra/pull/86)

## [v0.6.0] - 2024-01-29

**Build:**
* Avoid Boost autolinking libraries [#34](https://github.com/Autodesk/maya-hydra/pull/34)

**Documentation:**
* Update build doc & script [#37](https://github.com/Autodesk/maya-hydra/pull/37)

**Miscellaneous:**
* Fix Broken Link [#44](https://github.com/Autodesk/maya-hydra/pull/44)
* Make sure skydome light is not turned off unexpectedly [#41](https://github.com/Autodesk/maya-hydra/pull/41)
* Remove the usage of the "fit" mode for directional light unit test [#40](https://github.com/Autodesk/maya-hydra/pull/40)
* Fix the double transform issue on instanced prims [#39](https://github.com/Autodesk/maya-hydra/pull/39)
* Add prim instancing unit test [#33](https://github.com/Autodesk/maya-hydra/pull/33)
* Rename the env var to avoid duplication [#32](https://github.com/Autodesk/maya-hydra/pull/32)
* Fix testFlowViewportAPI image diff thresholds [#31](https://github.com/Autodesk/maya-hydra/pull/31)
* Fix for Maya Lighting mode not working [#30](https://github.com/Autodesk/maya-hydra/pull/30)
* Remove idiff workaround [#29](https://github.com/Autodesk/maya-hydra/pull/29)
* Fix scene browser crash when adding same prim multiple times [#28](https://github.com/Autodesk/maya-hydra/pull/28)
* Add missing rpaths to mayaHydraSceneBrowser [#27](https://github.com/Autodesk/maya-hydra/pull/27)
* Selection change functors now take scene index as argument [#26](https://github.com/Autodesk/maya-hydra/pull/26)
* Rename selection interface [#24](https://github.com/Autodesk/maya-hydra/pull/24)
* Fix added prims being at the wrong place when saving/loading a scene [#23](https://github.com/Autodesk/maya-hydra/pull/23)

## [v0.5.0] - 2023-12-07

**Build:**
* Report build and version information from the mayaHydra plugin [#10](https://github.com/Autodesk/maya-hydra/pull/10)
* Hydra for Maya has now a new home under https://github.com/Autodesk/maya-hydra

**Documentation:**
* Rename docs mentioning Flow Viewport Library to Flow Viewport Toolkit [#11](https://github.com/Autodesk/maya-hydra/pull/11)
* Update test README to clarify how to use MayaUSD in tests [#8](https://github.com/Autodesk/maya-hydra/pull/8)

**Miscellaneous:**
* Add filtering scene index interface to flow viewport toolkit [#21](https://github.com/Autodesk/maya-hydra/pull/21)
* Fix mesh adapter transform update [#20](https://github.com/Autodesk/maya-hydra/pull/20)
* Add data producer scene index interface in viewport API [#18](https://github.com/Autodesk/maya-hydra/pull/18)
* Add test for USD stage layer muting [#17](https://github.com/Autodesk/maya-hydra/pull/17)
* C++ 17, forward pointer declarations, and method naming [#16](https://github.com/Autodesk/maya-hydra/pull/16)
* Fix Hydra Scene Browser test [#15](https://github.com/Autodesk/maya-hydra/pull/15)
* Fix test for UFE lights from MayaUSD [#14](https://github.com/Autodesk/maya-hydra/pull/14)
* Flow selection GitHub [#9](https://github.com/Autodesk/maya-hydra/pull/9)
* Support USD v23.11 [#7](https://github.com/Autodesk/maya-hydra/pull/7)
* Merge viewport information interface [#4](https://github.com/Autodesk/maya-hydra/pull/4)
* Support multithreading in MayaHydraSceneIndex::RecreateAdapterOnIdle [#3](https://github.com/Autodesk/maya-hydra/pull/3)
* Fix directional and default lights not working [#2](https://github.com/Autodesk/maya-hydra/pull/2)

**Carry over from maya-usd repository:**
* [HYDRA-107] - mayaHydra: Maya crashes when loading an USD stage from file, then doing File->New
* [HYDRA-131] - mayaHydra: Arnold render delegate doesn't render IBL texture
* [HYDRA-181] - DomeLight attributes don't show an effect on lighting
* [HYDRA-185] - Selection Highlight for USD prims
* [HYDRA-399] - USD Proxy Node Transforms not captured by MayaHydra
* [HYDRA-414] - aiSkyDomeLight is shaded blue instead showing texture with Storm
* [HYDRA-449] - Prims with purpose set to default are not rendered
* [HYDRA-451] - Animation cache doesn't playback with mayaHydra
* [HYDRA-452] - Crash when deleting a prim from a USD stage in mayaHydra
* [HYDRA-465] - SceneIndex as default for Maya native scene data
* [HYDRA-469] - aiSkymeDomeLight : don't switch temporary textures when updating the color
* [HYDRA-485] - Hydra selection interface: Describe selection independent of SdfPaths
* [HYDRA-496] - Crash on exit or on New Scene when the viewport is active and there a USD stage
* [HYDRA-502] - Implement Custom MergingSceneIndex
* [HYDRA-511] - Hydra Scene Browser builds on Linux and Mac
* [HYDRA-529] - Failed to build MayaHydra with SceneBrowser plugin ON under Debug config
* [HYDRA-551] - USD Stage needs to be moved to be visible when using the viewport API
* [HYDRA-568] - Objects are not shown in the viewport when switching to MayaHydra from an empty scene
* [HYDRA-586] - Crash when switching to mayaHydra if the scene contains Maya data and mayaHydra had previously been used
* [HYDRA-613] - Crash when doing a file new with a callback using a dangling scene index
* [HYDRA-687] - MayaUsdUfeItems.skipUsdUfeLights test failing
  
## [v0.4.0] - 2023-08-30

**Build:**
- [HYDRA-453] - Update to USD 23.08

**Functionality:**
- [HYDRA-425] - Basic Hydra Scene Browser plugin available with mayaHydra
- [HYDRA-427] - Build up SceneIndex for Maya native scene
- [HYDRA-426] - Add new environment variable as switch for new SceneIndex approach
- [HYDRA-381] - Add support for input color space for DG nodes
- [HYDRA-447] - Default value for Display Purpose in Gloabal Settings
- [HYDRA-448] - Rename Global USD Display Purpose Tag in viewport settings
- [HYDRA-472] - Update Scene Indices chain code in maya-usd related to USD 23.08 integration

**Fixes:**
- [HYDRA-419] - Domelight texture not dirtying with SceneIndex update
- [HYDRA-180] - aiSkyDomeLight default color does not work
- [HYDRA-182] - UsdStage rprim of sphere type is not rendered
- [HYDRA-289] - Light intensity shows big difference compared to usdview
- [HYDRA-343] - Selecting native Maya nodes in the viewport selects the shape, not the transform
- [HYDRA-350] - Crash when assigning materialX standard surface to prim
- [HYDRA-413] - Referenced maya file with usd stage doesn't render in mayaHydra (Ill-formed SdfPath)
- [HYDRA-421] - Stage Display purpose setting has no effect
- [HYDRA-423] - Hydra Generative Plugin is not loaded in Maya
- [HYDRA-436] - Update issue muting USD layers
- [HYDRA-483] - Fix Scene Browser compilation for USD 23.08
- [HYDRA-493] - Lighting results differ between SceneIndex and SceneDelegate
- [HYDRA-494] - Shadows are not being cast on Maya native objects
- [HYDRA-320] - MaterialX shading/lighting issue

## [v0.3.0] - 2023-07-06

**Build:**
- [HYDRA-322] - Compilation failures when including terminalsResolvingSceneIndex.h

**Fixes:**
- [HYDRA-93] - Toggling between vp2 and Hydra stop updating Hydra
- [HYDRA-178] - USD instances are not shown in the mayaHydra viewport
- [HYDRA-184] - Wire frame and wire frame on shaded of USD objects not drawn
- [HYDRA-197] - Prevent scene delegate from being thrown out when we're switching back and forth between VP2 and hydra
- [HYDRA-287] - Hydra : OpenGL states not restored correctly
- [HYDRA-318] - Viewer doesn't update when working in USD stage
- [HYDRA-325] - Purpose defined prims don't render with mayaHydra
- [HYDRA-332] - Artifact with transparent surfaces in the viewport
- [HYDRA-342] - Rotation gizmo is missing its X/Y/Z axis
- [HYDRA-352] - Objects with namespaces will not draw in mayaHydra and switching back to VP2 will crash Maya
- [HYDRA-382] - Crash when going back to VP2 after loading PointMedcity.usd

## [v0.2.0] - 2023-05-03

**Build:**
- [HYDRA-333] - Update mayaHydra to USD 23.02
- [HYDRA-69] - Restructure directories in repository

**Performance:**
- [HYDRA-339] - Hydra hit test performance optimization via render a smaller region

**Miscellaneous:**
- [HYDRA-192] - remove UFE conditional compilation
- [HYDRA-321] - Added required ApiSchema for MaterialX bindings in USD
- [HYDRA-327] - Preserve existence of the previous selection temporarily inside of the Ufe Selection Notification (Clear, Replace)

**Fixes:**
- [HYDRA-79] - Crash when changing to HydraViewport for a USD referenced object
- [HYDRA-101] - Crash when switching from VP2 to Hydra on MacOS
- [HYDRA-109] - Loading the Arnold plug-in after Hydra causes the viewport to not update properly
- [HYDRA-116] - Meshes are not displayed as expected when switching between viewport modes
- [HYDRA-128] - The latest module template doesn't work
- [HYDRA-179] - MaterialX does not work with usd stage scene index
- [HYDRA-200] - Ghost mesh after deleting everything in the scene
- [HYDRA-211] - Require clang-format by automation
- [HYDRA-239] - Support for Apple Silicon
- [HYDRA-250] - Code path where ria is a nullptr and not tested.
- [HYDRA-266] - Undo, redo does not work with custom scene indices
- [HYDRA-283] - Crash when switching back and forth from VP2 to Hydra GL


## [v0.1.0] - 2022-03-27
- Initial release of Hydra for Maya (Technology Preview)
