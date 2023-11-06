# Changelog

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