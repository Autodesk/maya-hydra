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
//

#include "testUtils.h"

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/diagnosticMgr.h>
#include <pxr/imaging/hd/renderSettingsSchema.h>
#include <pxr/imaging/hd/sceneGlobalsSchema.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace {

// Collects the commentary of every TF_WARN issued while this delegate is registered, so a test
// can assert on warnings emitted by a single GetPrim() call without scraping stderr.
class _WarningCollector : public TfDiagnosticMgr::Delegate
{
public:
    void IssueError(TfError const&) override { }
    void IssueFatalError(TfCallContext const&, std::string const&) override { }

    void IssueWarning(TfWarning const& warning) override
    {
        _commentaries.push_back(warning.GetCommentary());
    }

    void IssueStatus(TfStatus const&) override { }

    bool AnyContains(const std::string& substring) const
    {
        for (const auto& commentary : _commentaries) {
            if (commentary.find(substring) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

private:
    std::vector<std::string> _commentaries;
};

// Re-trigger MhRenderingColorSpaceResolvingSceneIndex::GetPrim(), and return the
// resolved prim and any warnings issued while resolving it.
HdSceneIndexPrim _GetActiveRenderSettingsPrim(_WarningCollector& collector)
{
    const auto& si = GetTerminalSceneIndices();
    EXPECT_EQ(si.size(), 1u)
        << "Expected 1 terminal scene index, but found " << si.size();
    if (si.empty()) {
        return {};
    }
    // There should be only one terminal scene index in the vector, so use the last one.
    auto siRoot = si.back();

    HdSceneGlobalsSchema globalsSchema = HdSceneGlobalsSchema::GetFromSceneIndex(siRoot);
    EXPECT_TRUE(globalsSchema.IsDefined());
    if (!globalsSchema.IsDefined()) {
        return {};
    }

    auto activeRenderSettingsPrimDs = globalsSchema.GetActiveRenderSettingsPrim();
    EXPECT_TRUE(activeRenderSettingsPrimDs);
    if (!activeRenderSettingsPrimDs) {
        return {};
    }
    const SdfPath activePath = activeRenderSettingsPrimDs->GetTypedValue(0.0f);

    // Collect any warnings issued while resolving the prim.
    TfDiagnosticMgr::GetInstance().AddDelegate(&collector);
    HdSceneIndexPrim prim = siRoot->GetPrim(activePath);
    TfDiagnosticMgr::GetInstance().RemoveDelegate(&collector);

    return prim;
}

} // namespace

// Branch: renderingColorSpace not authored on the render settings prim -> silently fall back to
// Maya's rendering color space preference (ACEScg).
TEST(TestRenderingColorSpaceResolving, Unauthored)
{
    _WarningCollector collector;
    HdSceneIndexPrim  prim = _GetActiveRenderSettingsPrim(collector);

    auto rcs = HdRenderSettingsSchema::GetFromParent(prim.dataSource).GetRenderingColorSpace();
    ASSERT_TRUE(rcs);
    EXPECT_EQ(rcs->GetTypedValue(0.0f), TfToken("ACEScg"));
    EXPECT_FALSE(collector.AnyContains("doesn't match the value from Maya preferences"));
    EXPECT_FALSE(collector.AnyContains("is not known to the OCIO config file"));
}

// Branch: authored renderingColorSpace matches Maya's preference -> keep it, no warning.
TEST(TestRenderingColorSpaceResolving, Matching)
{
    _WarningCollector collector;
    HdSceneIndexPrim  prim = _GetActiveRenderSettingsPrim(collector);

    auto rcs = HdRenderSettingsSchema::GetFromParent(prim.dataSource).GetRenderingColorSpace();
    ASSERT_TRUE(rcs);
    EXPECT_EQ(rcs->GetTypedValue(0.0f), TfToken("ACEScg"));
    EXPECT_FALSE(collector.AnyContains("doesn't match the value from Maya preferences"));
    EXPECT_FALSE(collector.AnyContains("is not known to the OCIO config file"));
}

// Branch: authored renderingColorSpace is known to OCIO but differs from Maya's preference ->
// keep the authored value, warn.
TEST(TestRenderingColorSpaceResolving, Differing)
{
    _WarningCollector collector;
    HdSceneIndexPrim  prim = _GetActiveRenderSettingsPrim(collector);

    auto rcs = HdRenderSettingsSchema::GetFromParent(prim.dataSource).GetRenderingColorSpace();
    ASSERT_TRUE(rcs);
    EXPECT_EQ(rcs->GetTypedValue(0.0f), TfToken("scene-linear Rec.709-sRGB"));
    EXPECT_TRUE(collector.AnyContains("doesn't match the value from Maya preferences"));
}

// Branch: authored renderingColorSpace is not known to the OCIO config -> fall back to Maya's
// preference, warn.
TEST(TestRenderingColorSpaceResolving, Unknown)
{
    _WarningCollector collector;
    HdSceneIndexPrim  prim = _GetActiveRenderSettingsPrim(collector);

    auto rcs = HdRenderSettingsSchema::GetFromParent(prim.dataSource).GetRenderingColorSpace();
    ASSERT_TRUE(rcs);
    EXPECT_EQ(rcs->GetTypedValue(0.0f), TfToken("ACEScg"));
    EXPECT_TRUE(collector.AnyContains("is not known to the OCIO config file"));
}
