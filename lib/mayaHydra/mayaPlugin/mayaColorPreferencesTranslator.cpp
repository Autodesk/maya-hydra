// Copyright 2023 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "mayaColorPreferencesTranslator.h"

#include "colorNotFoundException.h"

#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/mixedUtils.h>

#include <maya/MEventMessage.h>

#include <flowViewport/colorPreferences/fvpColorChanged.h>
#include <flowViewport/colorPreferences/fvpColorPreferencesTokens.h>

namespace {
std::shared_ptr<MAYAHYDRA_NS_DEF::MayaColorPreferencesTranslator> instance;
} // namespace

namespace MAYAHYDRA_NS_DEF {

MayaColorPreferencesTranslator& MayaColorPreferencesTranslator::getInstance()
{
    if (!instance) {
        instance
            = std::shared_ptr<MayaColorPreferencesTranslator>(new MayaColorPreferencesTranslator());
    }
    return *instance;
}

void MayaColorPreferencesTranslator::deleteInstance()
{
    if (instance) {
        instance.reset();
    }
}

MayaColorPreferencesTranslator::MayaColorPreferencesTranslator()
{
    _callbackIds.append(MEventMessage::addEventCallback(
        "ColorIndexChanged", MayaColorPreferencesTranslator::onPreferencesChanged, this));
    _callbackIds.append(MEventMessage::addEventCallback(
        "DisplayColorChanged", MayaColorPreferencesTranslator::onPreferencesChanged, this));
    _callbackIds.append(MEventMessage::addEventCallback(
        "DisplayRGBColorChanged", MayaColorPreferencesTranslator::onPreferencesChanged, this));
    // Here is where we specify which colors get translated to the Flow Viewport.
    // trackRGBAColor will map a color name to a Flow Viewport token (and vice-versa).
    // trackIndexedColor will map a color name + table name to a Flow Viewport token (and
    // vice-versa).
    trackRGBAColor(kLeadColorName, PXR_NS::FvpColorPreferencesTokens->wireframeSelection);
    trackRGBAColor(
        kPolymeshActiveColorName, PXR_NS::FvpColorPreferencesTokens->wireframeSelectionSecondary);
    trackIndexedColor(
        kPolyVertexColorName,
        kActiveColorTableName,
        PXR_NS::FvpColorPreferencesTokens->vertexSelection);
    trackIndexedColor(
        kPolyEdgeColorName,
        kActiveColorTableName,
        PXR_NS::FvpColorPreferencesTokens->edgeSelection);
    trackIndexedColor(
        kPolyFaceColorName,
        kActiveColorTableName,
        PXR_NS::FvpColorPreferencesTokens->faceSelection);
}

MayaColorPreferencesTranslator::~MayaColorPreferencesTranslator()
{
    MMessage::removeCallbacks(_callbackIds);
}

void MayaColorPreferencesTranslator::trackRGBAColor(
    const std::string&     mayaColorName,
    const PXR_NS::TfToken& fvpColorToken)
{
    PXR_NS::GfVec4f color;
    if (getRGBAColorPreferenceValue(mayaColorName, color)) {
        MayaRGBAColor colorEntry = { mayaColorName, color };
        _rgbaColorsCache.emplace(fvpColorToken, colorEntry);
    } else {
        throw RGBAColorNotFoundException(mayaColorName);
    }
}

void MayaColorPreferencesTranslator::trackIndexedColor(
    const std::string&     mayaColorName,
    const std::string&     colorTableName,
    const PXR_NS::TfToken& fvpColorToken)
{
    PXR_NS::GfVec4f color;
    if (getIndexedColorPreferenceValue(mayaColorName, colorTableName, color)) {
        MayaIndexedColor colorEntry = { mayaColorName, colorTableName, color };
        _indexedColorsCache.emplace(fvpColorToken, colorEntry);
    } else {
        throw IndexedColorNotFoundException(mayaColorName, colorTableName);
    }
}

void MayaColorPreferencesTranslator::onPreferencesChanged(void* clientData)
{
    getInstance().syncPreferences();
}

void MayaColorPreferencesTranslator::syncPreferences()
{
    for (auto& rgbaColorMapping : _rgbaColorsCache) {
        MayaRGBAColor&  rgbaColorEntry = rgbaColorMapping.second;
        PXR_NS::GfVec4f currColor;
        if (getRGBAColorPreferenceValue(rgbaColorEntry.colorName, currColor)
            && currColor != rgbaColorEntry.color) {
            notify(Fvp::ColorChanged(rgbaColorMapping.first, rgbaColorEntry.color, currColor));
            rgbaColorEntry.color = currColor;
        }
    }

    for (auto& indexedColorMapping : _indexedColorsCache) {
        MayaIndexedColor& indexedColorEntry = indexedColorMapping.second;
        PXR_NS::GfVec4f   currColor;
        if (getIndexedColorPreferenceValue(
                indexedColorEntry.colorName, indexedColorEntry.tableName, currColor)
            && currColor != indexedColorEntry.color) {
            notify(
                Fvp::ColorChanged(indexedColorMapping.first, indexedColorEntry.color, currColor));
            indexedColorEntry.color = currColor;
        }
    }
}

bool MayaColorPreferencesTranslator::getColor(
    const PXR_NS::TfToken& preference,
    PXR_NS::GfVec4f&       outColor) const
{
    auto rgbaColorMapping = _rgbaColorsCache.find(preference);
    if (rgbaColorMapping != _rgbaColorsCache.end()) {
        outColor = rgbaColorMapping->second.color;
        return true;
    }

    auto indexedColorMapping = _indexedColorsCache.find(preference);
    if (indexedColorMapping != _indexedColorsCache.end()) {
        outColor = indexedColorMapping->second.color;
        return true;
    }

    return false;
}

} // namespace MAYAHYDRA_NS_DEF
