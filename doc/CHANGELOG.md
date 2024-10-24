# Changelog

## [v0.6.3] - 2024-09-10

**Build:**
* Support for controlled unit test disabling [#134](https://github.com/Autodesk/maya-hydra/pull/134)
* Test single picking [#145](https://github.com/Autodesk/maya-hydra/pull/145)
* Create a unit test for the shaded display mode with a texture image file [#151](https://github.com/Autodesk/maya-hydra/pull/151)
* Update maya-hydra-preflight-launcher.yml [#166](https://github.com/Autodesk/maya-hydra/pull/166)
 
**Flow Viewport Toolkit:**
* Isolate select scene index, with command-based implementation [#164](https://github.com/Autodesk/maya-hydra/pull/164)
 
**Miscellaneous:**
* Added missing space in resource path for Mac/Linux [#137](https://github.com/Autodesk/maya-hydra/pull/137)
* Added functionality for selection highlighting for point instancing of meshes [#132](https://github.com/Autodesk/maya-hydra/pull/132)
* Added convenience mayaHydra.h include to api.h [#136](https://github.com/Autodesk/maya-hydra/pull/136)
* Added code coverage crash workaround to prune textures scene index [#138](https://github.com/Autodesk/maya-hydra/pull/138)
* Added support picking & selecting faces for Maya meshes in Hydra viewport [#139](https://github.com/Autodesk/maya-hydra/pull/139)
* Handle hierarchical transform & visibility changes for plugin nodes [#141](https://github.com/Autodesk/maya-hydra/pull/141)
* Added a flow viewport example that uses a custom GLSLFX shader for Storm [#142](https://github.com/Autodesk/maya-hydra/pull/142)
* Added GeomSubsets picking filter + Fix multi-selection when single-picking [#143](https://github.com/Autodesk/maya-hydra/pull/143)
* Added support to use face selection material during Default Material override [#146](https://github.com/Autodesk/maya-hydra/pull/146)
* GeomSubsets wireframe highlighting [#148](https://github.com/Autodesk/maya-hydra/pull/148)
* Implemented custom data provider picking and selection highlighting [#150](https://github.com/Autodesk/maya-hydra/pull/150)
* Cmake changes pertaining to OneTBB update [#154](https://github.com/Autodesk/maya-hydra/pull/154)
* Refactored Repr Selector Scene Index to send Dirty Notification [#153](https://github.com/Autodesk/maya-hydra/pull/153)
* Added Maya command plugin to query build info [#163](https://github.com/Autodesk/maya-hydra/pull/163)
* Added unit test for Default Lighting with UsdStage [#168](https://github.com/Autodesk/maya-hydra/pull/168)
* Native instance prototypes picking/selection [#165](https://github.com/Autodesk/maya-hydra/pull/165)
* Updated build.md [#155](https://github.com/Autodesk/maya-hydra/pull/155)
* Added support for Maya lighting modes for USD and dome lights [#167](https://github.com/Autodesk/maya-hydra/pull/167)
 
**Bugfix:**
* Fixed aiSkyDomeLight bugs [#135](https://github.com/Autodesk/maya-hydra/pull/135)
* Fixed texture mode toggle on USD data [#144](https://github.com/Autodesk/maya-hydra/pull/144)
* Fixed Viewport Switch Crash [#156](https://github.com/Autodesk/maya-hydra/pull/156)
* Fixed instances selections being all wiped on an instancer when only one is removed [#158](https://github.com/Autodesk/maya-hydra/pull/158)
* Fixed manual lifetime management of MayaHydraSceneIndex's adapters [#161](https://github.com/Autodesk/maya-hydra/pull/161)
* Fixed crash when traversing prototype prims without prototype roots for highlighting [#159](https://github.com/Autodesk/maya-hydra/pull/159)
* Fixed issue when selecting the maya node of a scene producer scene index should highlight the prims [#162](https://github.com/Autodesk/maya-hydra/pull/162)
* Fixed face selection shader to behave as an unlit shader [#147](https://github.com/Autodesk/maya-hydra/pull/147)
 
**Documentation:**
* Created security.md file [#152](https://github.com/Autodesk/maya-hydra/pull/152)

## [v0.6.2] - 2024-06-06

**Build:**
* Viewport filters test [#124](https://github.com/Autodesk/maya-hydra/pull/124)
* Add support for text-based data source dump comparisons in tests [#88](https://github.com/Autodesk/maya-hydra/pull/88)
* Disable testMayaDisplayModes and testDataProducerSelHighlight [#129](https://github.com/Autodesk/maya-hydra/pull/129)

**Features:**
* Global default material override for Maya data [#117](https://github.com/Autodesk/maya-hydra/pull/117)
* Support wireframe and wire on shaded and support selection highlight with the right colors [#118](https://github.com/Autodesk/maya-hydra/pull/118)
* Added SceneIndex for texture toggling [#131](https://github.com/Autodesk/maya-hydra/pull/131)

**Miscellaneous:**
* Pass normals and tangents to Hydra from render item and mesh adapters [#116](https://github.com/Autodesk/maya-hydra/pull/116)
* Removed test sample file dependencies on Maya future release and Arnold [#121](https://github.com/Autodesk/maya-hydra/pull/121)
* Fix Linux CPU memory reporting [#127](https://github.com/Autodesk/maya-hydra/pull/127)
* Update to USD 24.05 [#126](https://github.com/Autodesk/maya-hydra/pull/126)
* Removed dependence on Maya version and mtoa plugin [#125](https://github.com/Autodesk/maya-hydra/pull/125)

**Bugfix:**
* Fixed time update, Bifrost stage update and USD stage file update [#120](https://github.com/Autodesk/maya-hydra/pull/120)

**Documentation:**
* Added tip for Python test development [#119](https://github.com/Autodesk/maya-hydra/pull/119)
* Update build doc [#123](https://github.com/Autodesk/maya-hydra/pull/123)

## [v0.6.1] - 2024-04-25

**Highlights:**
- Picking support for USD native instances and point instancers
- Picking now supports the selected USD Kind
- Complexity refinement is now available in the Hydra render settings
- New API example: [FootPrint node](lib/mayaHydra/flowViewportAPIExamples/footPrintNode)
- Basic bounding box display style
- MaterialX stack support for native Maya geometries

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
