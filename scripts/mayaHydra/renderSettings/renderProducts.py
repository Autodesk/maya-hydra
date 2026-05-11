# Copyright 2026 Autodesk, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Shared helpers for locating and iterating USD render products inside the
# Maya Hydra render-settings stage.  All render-setting overrides (-reg, -rd,
# -im, -of, ...) go through getRenderProductsToApplySettings() so that
# filtering logic lives in one place.

import mayaUsd.ufe as mayaUsdUfe

from pxr import Sdf, UsdRender

# Maya DAG path to the proxy shape that holds the render-settings USD stage.
_RS_STAGE_PATH = "|renderSettings|renderSettingsShape"

# USD prim path that is the parent of all render products in the
# render-settings stage (Maya Hydra convention: products live at /Render/<Name>).
_RS_RENDER_PRIM_PATH = Sdf.Path("/Render")


def getRenderSettingsStage():
    """Return the USD stage that holds the Maya Hydra render settings prim."""
    stage = mayaUsdUfe.getStage(_RS_STAGE_PATH)
    if not stage:
        raise RuntimeError(
            "No stage found at %s, getRenderSettingsStage() failed."
            % _RS_STAGE_PATH)
    return stage


def getRenderProductsToApplySettings():
    """Return the UsdRender.Product schema objects on which Maya Hydra
    render-setting overrides (-reg, -rd, -im, -of, ...) should be applied.

    This is the single selection point: future filtering (by product name,
    AOV, user choice, ...) belongs here so that every override goes through
    the same rule.

    Only direct children of /Render are scanned.  This matches the Maya Hydra
    convention of placing render products at /Render/<Name>; products nested
    deeper would be silently skipped."""
    stage = getRenderSettingsStage()

    rsParentPrim = stage.GetPrimAtPath(_RS_RENDER_PRIM_PATH)
    if not rsParentPrim:
        raise RuntimeError(
            "Render settings parent prim %s not found."
            % str(_RS_RENDER_PRIM_PATH))

    products = []
    for child in rsParentPrim.GetChildren():
        if child.IsA(UsdRender.Product):
            products.append(UsdRender.Product(child))
    return products


def applyToProductName(modifierFn):
    """Apply modifierFn to the productName attribute of every render product
    returned by getRenderProductsToApplySettings()."""
    for rp in getRenderProductsToApplySettings():
        # Calling Create() returns the existing attribute if there is one.
        pnAttr = rp.CreateProductNameAttr()
        if not pnAttr:
            raise RuntimeError(
                "Could not obtain a product name attribute for render product "
                "%s." % str(rp.GetPrim().GetPath()))
        pnAttr.Set(modifierFn(pnAttr.Get()))
