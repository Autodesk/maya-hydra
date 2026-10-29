#
# Copyright 2026 Autodesk
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

"""Renderer selection and reference-image lookup shared by MayaHydra tests."""

import os

TEST_RENDERER_ENV_VAR = "MAYAHYDRA_TEST_RENDERER"
TEST_RENDERER_STORM = "HdStorm"
TEST_RENDERER_FLASH = "HdFlash"

_TEST_RENDERER_DATA = {
    TEST_RENDERER_STORM: {
        "pluginName": "HdStormRendererPlugin",
        "referenceFolder": "storm",
        # Must match MtohRenderOverride::_RendererSupportsOutline().
        "outlineSelectionHighlighting": True,
    },
    TEST_RENDERER_FLASH: {
        "pluginName": "HdFlashRendererPlugin",
        "referenceFolder": "flash",
        "outlineSelectionHighlighting": True,
    },
}

def getRendererData(renderer):
    """Return the data associated with a renderer."""
    data = _TEST_RENDERER_DATA.get(renderer)
    if data is None:
        raise ValueError(
            "Unknown test renderer {!r}; expected one of {}.".format(
                renderer, sorted(_TEST_RENDERER_DATA)))
    return data

def getRendererPluginName(renderer):
    """Return the Hydra plugin name for a renderer."""
    return getRendererData(renderer)["pluginName"]

def getRendererOverrideName(renderer):
    """Return the Maya viewport override name for a renderer."""
    return "mayaHydraRenderOverride_" + getRendererPluginName(renderer)

def getRendererReferenceFolder(renderer):
    """Return the reference image folder for a renderer."""
    return getRendererData(renderer)["referenceFolder"]

def rendererSupportsOutlineSelectionHighlighting(renderer):
    """Return whether the plugin offers the outline selection highlight with a renderer."""
    return getRendererData(renderer).get("outlineSelectionHighlighting", False)

def getTestRenderer(environment=None):
    """Return the requested test renderer, defaulting to HdStorm."""
    environment = os.environ if environment is None else environment
    renderer = environment.get(TEST_RENDERER_ENV_VAR, TEST_RENDERER_STORM).strip()
    if renderer not in _TEST_RENDERER_DATA:
        raise ValueError(
            "Unknown {} value {!r}; expected one of {}.".format(
                TEST_RENDERER_ENV_VAR, renderer, sorted(_TEST_RENDERER_DATA)))
    return renderer

def getTestRendererPluginName(environment=None):
    """Return the Hydra plugin name for the test renderer."""
    return getRendererPluginName(getTestRenderer(environment))

def getTestRendererOverrideName(environment=None):
    """Return the Maya viewport override name for the test renderer."""
    return "mayaHydraRenderOverride_" + getTestRendererPluginName(environment)

def getTestRendererReferenceFolder(environment=None):
    """Return the reference image folder for the test renderer."""
    return getRendererReferenceFolder(getTestRenderer(environment))

def resolveReferenceImage(inputDir, renderer, mode, refImage, imageVersion=None):
    """Resolve an optional renderer/mode image before shared baselines.

    A renderer image is only needed when its pixels differ from the common
    baseline.  Keep imageVersion in every lookup candidate so version-specific
    references never fall back to an incompatible unversioned image.
    """
    if os.path.isabs(refImage):
        return refImage

    versionedImage = os.path.join(imageVersion, refImage) if imageVersion else refImage
    rendererFolder = _TEST_RENDERER_DATA[renderer]["referenceFolder"]
    candidates = (
        os.path.join(inputDir, rendererFolder, mode, versionedImage),
        os.path.join(inputDir, rendererFolder, versionedImage),
        os.path.join(inputDir, mode, versionedImage),
        os.path.join(inputDir, versionedImage),
    )
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    return candidates[-1]
