import mayaUsd.ufe as mayaUsdUfe

import maya.cmds as cmds
import maya.mel as mel

from pxr import Sdf, UsdRender

from pathlib import Path

def _setRenderProducts(newRenderProductFn):
    """Apply newRenderProductFn to each render product names to set a new render product name."""

    # Enumerate all USD render products under the render settings root prim.
    # Open the render settings stage.
    rsStagePathStr = "|renderSettings|renderSettingsShape"
    stage = mayaUsdUfe.getStage(rsStagePathStr)

    if not stage:
        raise RuntimeError("No stage found at %s, _setRenderProducts() failed." % rsStagePathStr)

    # Get the render settings prim.  Could also use
    #
    # rs = UsdRender.Settings.GetStageRenderSettings(stage)
    #
    # to get the render settings schema object from stage metadata.
    rsParentPrimPath = Sdf.Path("/Render")
    rsParentPrim = stage.GetPrimAtPath(rsParentPrimPath)
    
    if not rsParentPrim:
        raise RuntimeError("Render settings parent prim %s not found." % str(rsParentPrimPath))

    # rps is list of UsdRender.Product schema objects.
    rps = []
    for child in rsParentPrim.GetChildren():
        if child.IsA(UsdRender.Product):
            rps.append(UsdRender.Product(child))

    for rp in rps:
        # Get the product name attribute.  Calling Create() returns an
        # existing attribute.
        pnAttr = rp.CreateProductNameAttr()

        if not pnAttr:
            raise RuntimeError(
                "Could not obtain a product name attribute for render product "
                "%s." % str(rp.GetPrim().GetPath()))

        productName = pnAttr.Get()

        newProductName = newRenderProductFn(productName)

        pnAttr.Set(newProductName)


# As of 31-Mar-2026 we write these render settings both to Maya render
# settings and USD render settings.  This is a temporary measure to
# ease transition to USD render settings.

def setRenderDirectory(rd):
    # Set the Maya render settings.
    cmds.workspace(fr=['depth', rd])
    cmds.workspace(fr=['images', rd])

    # Given an input product name, map it to our argument directory.
    def setRenderProductName(productName):
        return str(Path(rd) / Path(productName).name)
        
    # Set the directory on all USD render products.
    _setRenderProducts(setRenderProductName)

def setImageName(im):
    # Set the Maya render setting.
    cmds.setAttr('defaultRenderGlobals.imageFilePrefix', im, type='string')

    def setRenderProductName(productName):
        p = Path(productName)
        return str(p.with_name(im + p.suffix))

    # Set the file name on all USD render products.
    _setRenderProducts(setRenderProductName)

def setOutputFormat(of):
    # Set the Maya render setting.
    mel.eval('setMayaSoftwareImageFormat("' + of + '")')
    cmds.setAttr('defaultArnoldDriver.aiTranslator', of, type='string')

    def setRenderProductName(productName):
        return str(Path(productName).with_suffix('.' + of))

    # Set the output format on all USD render products.
    _setRenderProducts(setRenderProductName)
