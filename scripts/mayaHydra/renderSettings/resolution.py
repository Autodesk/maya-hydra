import mayaUsd.ufe as mayaUsdUfe
import ufe

from pxr import Sdf, UsdRender, Gf

DEFAULT_WIDTH  = 960
DEFAULT_HEIGHT = 540

# Don't know where Python standard output is going, Render gobbles it up.
#
# with open("c:/temp/RenderOutput.txt", "w", encoding="utf-8") as f:
#     print("PPT: setWidth %s called." % str(width), file=f)

def getResolutionAttr():
    # Open the render settings stage.
    rsStagePathStr = "|renderSettings|renderSettingsShape"
    stage = mayaUsdUfe.getStage(rsStagePathStr)

    if not stage:
        raise RuntimeError("No stage found at %s, setWidth() failed." % rsStagePathStr)

    # Get the render settings prim.  Could also use
    #
    # rs = UsdRender.Settings.GetStageRenderSettings(stage)
    #
    # to get the render settings schema object from stage metadata.
    rsParentPrimPath = Sdf.Path("/Render")
    rsParentPrim = stage.GetPrimAtPath(rsParentPrimPath)
    
    if not rsParentPrim:
        raise RuntimeError("Render settings parent prim %s not found." % str(rsParentPrimPath))

    # rs is a UsdRender.Settings schema object.
    rs = None
    for child in rsParentPrim.GetChildren():
        if child.IsA(UsdRender.Settings):
            rs = UsdRender.Settings(child)
            break

    if not rs:
        raise RuntimeError("No render settings found under %s." % str(rsParentPrimPath))

    resAttr = rs.GetResolutionAttr()
    if (not resAttr):
        resAttr = rs.CreateResolutionAttr(
            (DEFAULT_WIDTH, DEFAULT_HEIGHT))

    return resAttr


def setWidth(width):
    resAttr = getResolutionAttr()
    w, h = resAttr.Get()

    # Passing a Python integer 2-tuple directly to Set() causes
    # a Gf.Vec2d() to be created, and a type mismatch when the
    # Python C++ binding is called.  PPT, 3-Feb-2026.
    newRes = Gf.Vec2i(width, h)
    resAttr.Set(newRes)

def setHeight(height):
    resAttr = getResolutionAttr()
    w, h = resAttr.Get()

    # See setWidth() Gf.Vec2i comments.
    newRes = Gf.Vec2i(w, height)
    resAttr.Set(newRes)
