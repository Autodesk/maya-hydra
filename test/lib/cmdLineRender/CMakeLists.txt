# Command line render unit test scripts, launched with Render.exe.
# Unit test can be disabled with ctest labels.
# Adding labels to the unit test follows this convention:
# myTestFile.ma|depOnPlugins:lookdev,mtoa,newPlugin|skipOnPlatform:[osx,win,lin]
# See accompanying README.md for currently supported list of label mapping.
# only single platform exclusion is supported currently.
# For specifying exclusion based on plugin dependency, look at: test_to_run.json
set(STORM_RENDER_SCENES
    basic/sphere.ma
)

foreach(scene ${STORM_RENDER_SCENES})
    mayaHydra_add_cmd_line_render_test(${scene})
endforeach()
