// Copyright 2026 Autodesk
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

#include "testUtils.h"

#include <pxr/imaging/hd/sceneGlobalsSchema.h>

#include <ufe/pathString.h>

#include <flowViewport/selection/fvpPathMapperRegistry.h>
#include <gtest/gtest.h>

#include <string>


PXR_NAMESPACE_USING_DIRECTIVE

namespace {

struct _ActiveRenderDescriptionPaths
{
    SdfPath renderSettings;
    SdfPath renderPass;
};

_ActiveRenderDescriptionPaths _GetActiveRenderDescriptionPaths()
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    EXPECT_EQ(sceneIndices.size(), 1u)
        << "Expected one terminal scene index, but found " << sceneIndices.size();
    if (sceneIndices.size() != 1) {
        return {};
    }

    const HdSceneGlobalsSchema globals
        = HdSceneGlobalsSchema::GetFromSceneIndex(sceneIndices.front());
    EXPECT_TRUE(globals.IsDefined());
    if (!globals.IsDefined()) {
        return {};
    }

    const HdPathDataSourceHandle renderSettings = globals.GetActiveRenderSettingsPrim();
    const HdPathDataSourceHandle renderPass = globals.GetActiveRenderPassPrim();
    EXPECT_TRUE(renderSettings);
    EXPECT_TRUE(renderPass);
    if (!renderSettings || !renderPass) {
        return {};
    }

    return {
        renderSettings->GetTypedValue(0.0f),
        renderPass->GetTypedValue(0.0f),
    };
}

SdfPath _GetSceneIndexPath(const char* usdPrimPath)
{
    const Ufe::Path appPath
        = Ufe::PathString::path(std::string("|renderSettings|renderSettingsShape,") + usdPrimPath);
    const SdfPath result = Fvp::sceneIndexPath(appPath);
    EXPECT_FALSE(result.IsEmpty());
    return result;
}

SdfPath _GetDefaultRenderSettingsPath()
{
    const Ufe::Path appPath
        = Ufe::PathString::path("UsdDefaultRenderDescription,/Render/SceneRenderSettings");
    const SdfPath result = Fvp::sceneIndexPath(appPath);
    EXPECT_FALSE(result.IsEmpty());
    return result;
}

} // namespace

TEST(TestActiveRenderDescriptionSceneGlobals, RenderSettings)
{
    const auto active = _GetActiveRenderDescriptionPaths();

    EXPECT_EQ(active.renderSettings, _GetSceneIndexPath("/Render/MainRender"));
    EXPECT_TRUE(active.renderPass.IsEmpty());
}

TEST(TestActiveRenderDescriptionSceneGlobals, ValidRenderPass)
{
    const auto active = _GetActiveRenderDescriptionPaths();

    EXPECT_EQ(active.renderSettings, _GetSceneIndexPath("/Render/MainRender"));
    EXPECT_EQ(active.renderPass, _GetSceneIndexPath("/Render/Passes/foreground"));
}

TEST(TestActiveRenderDescriptionSceneGlobals, InvalidRenderPassSource)
{
    const auto active = _GetActiveRenderDescriptionPaths();

    EXPECT_EQ(active.renderSettings, _GetDefaultRenderSettingsPath());
    EXPECT_EQ(active.renderPass, _GetSceneIndexPath("/Render/Passes/foreground"));
}

TEST(TestActiveRenderDescriptionSceneGlobals, MissingRenderPass)
{
    const auto active = _GetActiveRenderDescriptionPaths();

    EXPECT_EQ(active.renderSettings, _GetDefaultRenderSettingsPath());
    EXPECT_TRUE(active.renderPass.IsEmpty());
}
