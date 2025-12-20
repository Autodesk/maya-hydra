# Command line render unit test scripts, launched with Render.exe.
# Unit test can be disabled with ctest labels.
# Adding labels to the unit test follows this convention:
# myTestFile.ma|depOnPlugins:lookdev,mtoa,newPlugin|skipOnPlatform:[osx,win,lin]
# See accompanying README.md for currently supported list of label mapping.
# only single platform exclusion is supported currently.
# For specifying exclusion based on plugin dependency, look at: test_to_run.json
if(IS_WINDOWS)
set(STORM_RENDER_SCENES
    basic/sphere.ma
    texturing/mayaSphereMayaMaterialStorm.ma
# Disabled because loaded MaterialX file cannot be relative to Maya file.
#   texturing/mayaSphereMaterialXStorm.ma
    texturing/usdSphereMaterialXStorm.ma
    texturing/usdSphereUsdMaterialStorm.ma
)

set(ARNOLD_RENDER_SCENES
    texturing/mayaSphereMayaMaterialArnold.ma
# Disabled because loaded MaterialX file cannot be relative to Maya file.
#   texturing/mayaSphereMaterialXArnold.ma
    texturing/usdSphereMaterialXArnold.ma
    texturing/usdSphereUsdMaterialArnold.ma
)
else()
# HYDRA-2031 cmdLineRender tests fail on macOS and Linux.
set(STORM_RENDER_SCENES "")
set(ARNOLD_RENDER_SCENES "")
endif()

foreach(scene ${STORM_RENDER_SCENES})
    mayaHydra_add_cmd_line_render_test(${scene})
endforeach()

foreach(scene ${ARNOLD_RENDER_SCENES})
    mayaHydra_add_cmd_line_render_test(${scene}
    RENDERER hydraArnold
)
endforeach()
