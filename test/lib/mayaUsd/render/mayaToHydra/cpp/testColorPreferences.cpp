//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/mixedUtils.h>

#include <maya/M3dView.h>
#include <maya/MGlobal.h>

#include <flowViewport/colorPreferences/fvpColorChanged.h>
#include <flowViewport/colorPreferences/fvpColorPreferences.h>
#include <flowViewport/colorPreferences/fvpColorPreferencesTokens.h>
#include <gtest/gtest.h>

#include <cmath>
#include <sstream>

// TODO : When/if we figure out how to put tokens in a custom namespace,
// remove this and add the relevant namespace at the callsites.
using PXR_NS::FvpColorPreferencesTokens;

using namespace MayaHydra;

namespace {

class ColorPreferencesTestObserver : public Fvp::Observer
{
public:
    ~ColorPreferencesTestObserver() override = default;

    void operator()(const Fvp::Notification& notification) override
    {
        _notifications.push_back(notification.staticCast<Fvp::ColorChanged>());
    }

    const std::vector<Fvp::ColorChanged>& getReceivedNotifications() { return _notifications; }

private:
    std::vector<Fvp::ColorChanged> _notifications;
};

double kDefaultColorDifferenceTolerance = 1e-4;
double kDefaultColorComponentShift = 0.1;

bool colorsAreClose(
    const PXR_NS::GfVec4f color1,
    const PXR_NS::GfVec4f color2,
    double                tolerance = kDefaultColorDifferenceTolerance)
{
    return PXR_NS::GfIsClose(color1, color2, tolerance);
}

void changeRGBAColor(const std::string& colorName, const PXR_NS::GfVec4f& colorValue)
{
    std::stringstream colorValueStringStream;
    colorValueStringStream << colorValue[0] << " " << colorValue[1] << " " << colorValue[2] << " "
                           << colorValue[3];
    std::string command = "displayRGBColor " + colorName + " " + colorValueStringStream.str();
    MGlobal::executeCommand(MString(command.c_str()));
}

void changeColorIndex(const std::string& colorName, const std::string& tableName, size_t newIndex)
{
    std::string changeIndexCommand
        = "displayColor -" + tableName + " " + colorName + " " + std::to_string(newIndex);
    MGlobal::executeCommand(MString(changeIndexCommand.c_str()));
}

void changePaletteColor(
    const std::string&     colorName,
    const std::string&     tableName,
    const PXR_NS::GfVec4f& colorValue)
{
    std::stringstream colorValueStringStream;
    colorValueStringStream << colorValue[0] << " " << colorValue[1] << " " << colorValue[2];

    size_t indexInPalette = 0;
    if (!getIndexedColorPreferenceIndex(colorName, tableName, indexInPalette)) {
        return;
    }

    std::string changePaletteColorCommand = "colorIndex -" + tableName + " "
        + std::to_string(indexInPalette) + " " + colorValueStringStream.str();
    MGlobal::executeCommand(MString(changePaletteColorCommand.c_str()));
}

PXR_NS::GfVec4f getShiftedColorComponents(
    const PXR_NS::GfVec4f& colorToShift,
    size_t                 nbComponentsToShift,
    double                 shift = kDefaultColorComponentShift)
{
    PXR_NS::GfVec4f shiftedColor = colorToShift;
    for (size_t iColorComponent = 0; iColorComponent < nbComponentsToShift; iColorComponent++) {
        shiftedColor[iColorComponent] += shift;
        if (shiftedColor[iColorComponent] > 1.0) {
            // Keep the resulting value within [0,1]
            shiftedColor[iColorComponent] -= std::floor(shiftedColor[iColorComponent]);
        }
    }
    return shiftedColor;
}

PXR_NS::GfVec4f getShiftedRGBComponents(
    const PXR_NS::GfVec4f& colorToShift,
    double                 shift = kDefaultColorComponentShift)
{
    return getShiftedColorComponents(colorToShift, 3, shift);
}

PXR_NS::GfVec4f getShiftedRGBAComponents(
    const PXR_NS::GfVec4f& colorToShift,
    double                 shift = kDefaultColorComponentShift)
{
    return getShiftedColorComponents(colorToShift, 4, shift);
}

size_t getShiftedColorIndex(size_t originalColorIndex)
{
    // We want to cycle through the colors in the palette. However, their indices are 1-based, so we
    // shift back to 0-based for the modulo and then back to 1-based. We do -1 to make the index
    // 0-based, +1 to increment it, % by the number of colors to keep it within range, then +1 to
    // make it 1-based again. The first -1 and +1 cancel out, but I left them to be explicit.
    MStatus isValid3dView;
    M3dView active3dView = M3dView::active3dView(&isValid3dView);
    EXPECT_TRUE(isValid3dView);
    MStatus      isValidColorCount;
    unsigned int nbColorsInTable = active3dView.numActiveColors(&isValidColorCount);
    EXPECT_TRUE(isValidColorCount);
    size_t newColorIndex = ((originalColorIndex - 1 + 1) % nbColorsInTable) + 1;
    return newColorIndex;
}

} // namespace

TEST(ColorPreferences, rgbaColorNotification)
{
    PXR_NS::GfVec4f initialColor;
    EXPECT_TRUE(getRGBAColorPreferenceValue(kLeadColorName, initialColor));

    // Hook up the observer to the Flow color preferences
    auto observer = std::make_shared<ColorPreferencesTestObserver>();
    Fvp::ColorPreferences::getInstance().addObserver(observer);

    // Change the Maya color to trigger a notification
    PXR_NS::GfVec4f newColor = getShiftedRGBAComponents(initialColor);
    changeRGBAColor(kLeadColorName, newColor);

    // Must explicitly remove observer, otherwise invalid read can occur. The observer's
    // shared_ptr control block is allocated in the current test DLL, but can linger around
    // in the UFE DLL through a weak_ptr if not explicitly removed. If the test DLL gets
    // unloaded, and the UFE DLL tries to delete the control block, an invalid read will occur.
    Fvp::ColorPreferences::getInstance().removeObserver(observer);

    // Check notification info
    ASSERT_EQ(observer->getReceivedNotifications().size(), 1u);
    Fvp::ColorChanged notification = observer->getReceivedNotifications().back();
    EXPECT_EQ(notification.token(), FvpColorPreferencesTokens->wireframeSelection);
    EXPECT_TRUE(colorsAreClose(notification.oldColor(), initialColor));
    EXPECT_TRUE(colorsAreClose(notification.newColor(), newColor));
}

TEST(ColorPreferences, indexedColorNotification)
{
    PXR_NS::GfVec4f initialColor;
    EXPECT_TRUE(
        getIndexedColorPreferenceValue(kPolyVertexColorName, kActiveColorTableName, initialColor));

    // Hook up the observer to the Flow color preferences
    auto observer = std::make_shared<ColorPreferencesTestObserver>();
    Fvp::ColorPreferences::getInstance().addObserver(observer);

    // Change the Maya color index to trigger a notification
    size_t previousColorIndex;
    EXPECT_TRUE(getIndexedColorPreferenceIndex(
        kPolyVertexColorName, kActiveColorTableName, previousColorIndex));
    size_t newColorIndex = getShiftedColorIndex(previousColorIndex);
    changeColorIndex(kPolyVertexColorName, kActiveColorTableName, newColorIndex);

    // Must explicitly remove observer, otherwise invalid read can occur. The observer's
    // shared_ptr control block is allocated in the current test DLL, but can linger around
    // in the UFE DLL through a weak_ptr if not explicitly removed. If the test DLL gets
    // unloaded, and the UFE DLL tries to delete the control block, an invalid read will occur.
    Fvp::ColorPreferences::getInstance().removeObserver(observer);

    PXR_NS::GfVec4f newColor;
    EXPECT_TRUE(
        getIndexedColorPreferenceValue(kPolyVertexColorName, kActiveColorTableName, newColor));

    // Check notification info
    ASSERT_EQ(observer->getReceivedNotifications().size(), 1u);
    Fvp::ColorChanged notification = observer->getReceivedNotifications().back();
    EXPECT_EQ(notification.token(), FvpColorPreferencesTokens->vertexSelection);
    EXPECT_TRUE(colorsAreClose(notification.oldColor(), initialColor));
    EXPECT_TRUE(colorsAreClose(notification.newColor(), newColor));
}

TEST(ColorPreferences, paletteColorNotification)
{
    PXR_NS::GfVec4f initialColor;
    EXPECT_TRUE(
        getIndexedColorPreferenceValue(kPolyVertexColorName, kActiveColorTableName, initialColor));

    // Hook up the observer to the Flow color preferences
    auto observer = std::make_shared<ColorPreferencesTestObserver>();
    Fvp::ColorPreferences::getInstance().addObserver(observer);

    // Change the Maya color in the palette to trigger a notification
    PXR_NS::GfVec4f newColor = getShiftedRGBComponents(initialColor);
    changePaletteColor(kPolyVertexColorName, kActiveColorTableName, newColor);

    // Must explicitly remove observer, otherwise invalid read can occur. The observer's
    // shared_ptr control block is allocated in the current test DLL, but can linger around
    // in the UFE DLL through a weak_ptr if not explicitly removed. If the test DLL gets
    // unloaded, and the UFE DLL tries to delete the control block, an invalid read will occur.
    Fvp::ColorPreferences::getInstance().removeObserver(observer);

    // Check notification info
    ASSERT_EQ(observer->getReceivedNotifications().size(), 1u);
    Fvp::ColorChanged notification = observer->getReceivedNotifications().back();
    EXPECT_EQ(notification.token(), FvpColorPreferencesTokens->vertexSelection);
    EXPECT_TRUE(colorsAreClose(notification.oldColor(), initialColor));
    EXPECT_TRUE(colorsAreClose(notification.newColor(), newColor));
}

TEST(ColorPreferences, rgbaColorQuery)
{
    // Query color from Maya
    PXR_NS::GfVec4f initialMayaColor;
    EXPECT_TRUE(getRGBAColorPreferenceValue(kLeadColorName, initialMayaColor));

    // Query color from Flow Viewport
    PXR_NS::GfVec4f initialFvpColor;
    EXPECT_TRUE(Fvp::ColorPreferences::getInstance().getColor(
        FvpColorPreferencesTokens->wireframeSelection, initialFvpColor));

    // Check that the queried colors match
    EXPECT_TRUE(colorsAreClose(initialFvpColor, initialMayaColor));

    // Change the Maya color
    PXR_NS::GfVec4f newMayaColor = getShiftedRGBAComponents(initialMayaColor);
    changeRGBAColor(kLeadColorName, newMayaColor);

    // Check that a new Flow Viewport color query is correct
    PXR_NS::GfVec4f newFvpColor;
    EXPECT_TRUE(Fvp::ColorPreferences::getInstance().getColor(
        FvpColorPreferencesTokens->wireframeSelection, newFvpColor));
    EXPECT_TRUE(colorsAreClose(newFvpColor, newMayaColor));
}

TEST(ColorPreferences, indexedColorQuery)
{
    // Query color from Maya
    PXR_NS::GfVec4f initialMayaColor;
    EXPECT_TRUE(getIndexedColorPreferenceValue(
        kPolyVertexColorName, kActiveColorTableName, initialMayaColor));

    // Query color from Flow Viewport
    PXR_NS::GfVec4f initialFvpColor;
    EXPECT_TRUE(Fvp::ColorPreferences::getInstance().getColor(
        FvpColorPreferencesTokens->vertexSelection, initialFvpColor));

    // Check that the queried colors match
    EXPECT_TRUE(colorsAreClose(initialFvpColor, initialMayaColor));

    // Change the Maya palette index of the color
    size_t previousColorIndex;
    EXPECT_TRUE(getIndexedColorPreferenceIndex(
        kPolyVertexColorName, kActiveColorTableName, previousColorIndex));
    size_t newColorIndex = getShiftedColorIndex(previousColorIndex);
    changeColorIndex(kPolyVertexColorName, kActiveColorTableName, newColorIndex);

    // Compare Maya and Flow Viewport-retrieved colors
    PXR_NS::GfVec4f newMayaColor;
    EXPECT_TRUE(
        getColorPreferencesPaletteColor(kActiveColorTableName, newColorIndex, newMayaColor));
    PXR_NS::GfVec4f newFvpColor;
    EXPECT_TRUE(Fvp::ColorPreferences::getInstance().getColor(
        FvpColorPreferencesTokens->vertexSelection, newFvpColor));
    EXPECT_TRUE(colorsAreClose(newFvpColor, newMayaColor));
}

TEST(ColorPreferences, paletteColorQuery)
{
    // Query color from Maya
    PXR_NS::GfVec4f initialMayaColor;
    EXPECT_TRUE(getIndexedColorPreferenceValue(
        kPolyVertexColorName, kActiveColorTableName, initialMayaColor));

    // Query color from Flow Viewport
    PXR_NS::GfVec4f initialFvpColor;
    EXPECT_TRUE(Fvp::ColorPreferences::getInstance().getColor(
        FvpColorPreferencesTokens->vertexSelection, initialFvpColor));

    // Check that the queried colors match
    EXPECT_TRUE(colorsAreClose(initialFvpColor, initialMayaColor));

    // Change the Maya color in the palette
    PXR_NS::GfVec4f newMayaColor = getShiftedRGBComponents(initialMayaColor);
    changePaletteColor(kPolyVertexColorName, kActiveColorTableName, newMayaColor);

    // Check that a new Flow Viewport color query is correct
    PXR_NS::GfVec4f newFvpColor;
    EXPECT_TRUE(Fvp::ColorPreferences::getInstance().getColor(
        FvpColorPreferencesTokens->vertexSelection, newFvpColor));
    EXPECT_TRUE(colorsAreClose(newFvpColor, newMayaColor));
}
