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
// Migration gate for FvpDirtyNotifier: for each representative HdDirtyBits combination,
// compares HdDirtyBitsTranslator output to the equivalent notifier emission. Deliberately
// narrower cases (extComputation primvars, light params) assert explicit expected sets.
// Pure USD/Hd + flowViewport logic with no Maya dependency.

#include <gtest/gtest.h>

#include <flowViewport/fvpDirtyNotifier.h>

#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/changeTracker.h>
#include <pxr/imaging/hd/dataSourceLocator.h>
#include <pxr/imaging/hd/dirtyBitsTranslator.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/tokens.h>

#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/basisCurvesTopologySchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/subdivisionTagsSchema.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/xformSchema.h>

#include <functional>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const SdfPath kTestPrimPath("/dirtyNotifierTestPrim");

// Apply a set of dirty*() calls to a fresh notifier and return the accumulated
// locator set. Flushes afterwards so the explicit-flush contract is honored.
HdDataSourceLocatorSet
NotifierLocators(const std::function<void(Fvp::FvpDirtyNotifier&)>& applyDirty)
{
    HdRetainedSceneIndexRefPtr sceneIndex = HdRetainedSceneIndex::New();
    HdDataSourceLocatorSet     result;
    {
        Fvp::FvpDirtyNotifier notifier(*sceneIndex, kTestPrimPath);
        applyDirty(notifier);
        result = notifier.GetLocators();
        notifier.flush();
    }
    return result;
}

HdDataSourceLocatorSet
TranslatorRprimLocators(const TfToken& primType, HdDirtyBits bits)
{
    HdDataSourceLocatorSet set;
    HdDirtyBitsTranslator::RprimDirtyBitsToLocatorSet(primType, bits, &set);
    // _MarkPrimDirty appended the broad primvars locator for DirtyPrimvar; mirror
    // that so comparisons reflect the actual legacy emission path.
    if (bits & HdChangeTracker::DirtyPrimvar) {
        set.append(HdPrimvarsSchema::GetDefaultLocator());
    }
    return set;
}

HdDataSourceLocatorSet
TranslatorSprimLocators(const TfToken& primType, HdDirtyBits bits)
{
    HdDataSourceLocatorSet set;
    HdDirtyBitsTranslator::SprimDirtyBitsToLocatorSet(primType, bits, &set);
    if (bits & HdChangeTracker::DirtyPrimvar) {
        set.append(HdPrimvarsSchema::GetDefaultLocator());
    }
    return set;
}

} // namespace

// ---- Cases that must match the legacy translator exactly ----

TEST(FvpDirtyNotifier, transformMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyTransform),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyTransform(); }));
}

TEST(FvpDirtyNotifier, visibilityMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyVisibility),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyVisibility(); }));
}

TEST(FvpDirtyNotifier, doubleSidedMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyDoubleSided),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyDoubleSided(); }));
}

TEST(FvpDirtyNotifier, materialBindingMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyMaterialId),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyMaterialBinding(); }));
}

TEST(FvpDirtyNotifier, meshTopologyMatchesTranslator)
{
    // Rule 2: topology only - no primvars, no extent.
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyTopology),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyMeshTopology(); }));
}

TEST(FvpDirtyNotifier, basisCurvesTopologyMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->basisCurves, HdChangeTracker::DirtyTopology),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyBasisCurvesTopology(); }));
}

TEST(FvpDirtyNotifier, dirtyTopologyDispatchesByPrimType)
{
    EXPECT_EQ(
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyMeshTopology(); }),
        NotifierLocators(
            [](Fvp::FvpDirtyNotifier& n) { n.dirtyTopology(HdPrimTypeTokens->mesh); }));
    EXPECT_EQ(
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyBasisCurvesTopology(); }),
        NotifierLocators(
            [](Fvp::FvpDirtyNotifier& n) { n.dirtyTopology(HdPrimTypeTokens->basisCurves); }));
}

TEST(FvpDirtyNotifier, rprimConnectivityChangeOmitsNormalsByDefault)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdMeshSchema::GetSubdivisionSchemeLocator());
    expected.append(HdMeshTopologySchema::GetDefaultLocator());
    expected.append(HdPrimvarsSchema::GetDefaultLocator());
    expected.append(HdPrimvarsSchema::GetPointsLocator());
    expected.append(HdExtentSchema::GetDefaultLocator());

    EXPECT_EQ(
        expected,
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) {
            Fvp::FvpDirtyNotifier::DirtyRprimConnectivityLocators(n, HdPrimTypeTokens->mesh);
        }));
}

TEST(FvpDirtyNotifier, rprimConnectivityChangeOmitsGranularFaceVaryingPrimvars)
{
    HdDataSourceLocatorSet actual = NotifierLocators([](Fvp::FvpDirtyNotifier& n) {
        Fvp::FvpDirtyNotifier::DirtyRprimConnectivityLocators(n, HdPrimTypeTokens->mesh);
    });
    // Connectivity emits broad primvars (which intersects child locators), but must not
    // add redundant explicit face-varying primvar generators on top.
    const HdDataSourceLocator stLocator
        = HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("st"));
    const HdDataSourceLocator tangentsLocator
        = HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("tangents"));
    for (const auto& loc : actual) {
        EXPECT_NE(loc, HdPrimvarsSchema::GetNormalsLocator());
        EXPECT_NE(loc, stLocator);
        EXPECT_NE(loc, tangentsLocator);
    }
}

TEST(FvpDirtyNotifier, basisCurvesConnectivityChangeOmitsMeshTopology)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdBasisCurvesTopologySchema::GetDefaultLocator());
    expected.append(HdPrimvarsSchema::GetDefaultLocator());
    expected.append(HdPrimvarsSchema::GetPointsLocator());
    expected.append(HdExtentSchema::GetDefaultLocator());

    EXPECT_EQ(
        expected,
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) {
            Fvp::FvpDirtyNotifier::DirtyRprimConnectivityLocators(
                n, HdPrimTypeTokens->basisCurves);
        }));
}

TEST(FvpDirtyNotifier, rprimConnectivityChangeUnsupportedPrimTypeNoOps)
{
    EXPECT_TRUE(NotifierLocators([](Fvp::FvpDirtyNotifier& n) {
        Fvp::FvpDirtyNotifier::DirtyRprimConnectivityLocators(n, HdPrimTypeTokens->points);
    }).IsEmpty());
}

TEST(FvpDirtyNotifier, smoothMeshDisplayOmitsNormalsByDefault)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdLegacyDisplayStyleSchema::GetDefaultLocator());
    expected.append(HdMeshSchema::GetSubdivisionSchemeLocator());
    expected.append(HdMeshTopologySchema::GetDefaultLocator());
    expected.append(HdSubdivisionTagsSchema::GetDefaultLocator());

    EXPECT_EQ(
        expected,
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) {
            Fvp::FvpDirtyNotifier::DirtySmoothMeshDisplayLocators(n);
        }));
}

TEST(FvpDirtyNotifier, smoothMeshDisplayOmitsGranularFaceVaryingPrimvars)
{
    HdDataSourceLocatorSet actual = NotifierLocators([](Fvp::FvpDirtyNotifier& n) {
        Fvp::FvpDirtyNotifier::DirtySmoothMeshDisplayLocators(n);
    });
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetNormalsLocator()));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("st"))));
    EXPECT_FALSE(
        actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("tangents"))));
}

TEST(FvpDirtyNotifier, extentMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyExtent),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyExtent(); }));
}

TEST(FvpDirtyNotifier, cullStyleMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyCullStyle),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyCullStyle(); }));
}

TEST(FvpDirtyNotifier, displayStyleMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyDisplayStyle),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyDisplayStyle(); }));
}

TEST(FvpDirtyNotifier, subdivisionMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtySubdivTags),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtySubdivision(); }));
}

TEST(FvpDirtyNotifier, pointsMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyPoints),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyPoints(); }));
}

TEST(FvpDirtyNotifier, normalsMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyNormals),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyNormals(); }));
}

TEST(FvpDirtyNotifier, cameraParamsMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorSprimLocators(HdPrimTypeTokens->camera, HdCamera::DirtyParams),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyCameraParams(); }));
}

TEST(FvpDirtyNotifier, cameraTransformMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorSprimLocators(HdPrimTypeTokens->camera, HdCamera::DirtyTransform),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyTransform(); }));
}

TEST(FvpDirtyNotifier, materialMatchesTranslator)
{
    // HdMaterial::DirtyVolume and HdChangeTracker::DirtyPrimvar share bit 6, so
    // AllDirty would make TranslatorSprimLocators also append primvars. Any other
    // material dirty bit maps to the material schema locator only.
    EXPECT_EQ(
        TranslatorSprimLocators(HdPrimTypeTokens->material, HdMaterial::DirtyParams),
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyMaterial(); }));
}

TEST(FvpDirtyNotifier, instancerMatchesTranslator)
{
    HdDataSourceLocatorSet expected;
    HdDirtyBitsTranslator::InstancerDirtyBitsToLocatorSet(
        TfToken(),
        HdChangeTracker::DirtyInstancer | HdChangeTracker::DirtyInstanceIndex,
        &expected);
    EXPECT_EQ(expected, NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyInstancer(); }));
}

// ---- Cases where the notifier is deliberately narrower than the translator ----

TEST(FvpDirtyNotifier, primvarsOmitsExtComputation)
{
    // The translator also emits HdExtComputationPrimvarsSchema for DirtyPrimvar,
    // but the Maya adapters do not use ext-computation primvars. The broad
    // dirtyPrimvars() must emit only the primvars locator.
    HdDataSourceLocatorSet expected;
    expected.append(HdPrimvarsSchema::GetDefaultLocator());
    EXPECT_EQ(expected, NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyPrimvars(); }));
}

TEST(FvpDirtyNotifier, lightParamsIsGranular)
{
    // dirtyLightParams() intentionally emits ONLY the light schema locator. The
    // translator additionally emits primvars/visibility/collections for
    // DirtyParams; those are the responsibility of the call site.
    HdDataSourceLocatorSet expected;
    expected.append(HdLightSchema::GetDefaultLocator());
    EXPECT_EQ(expected, NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyLightParams(); }));
}

TEST(FvpDirtyNotifier, materialSkipsPrimvarsOnAllDirty)
{
    // HdMaterial::DirtyVolume and HdChangeTracker::DirtyPrimvar share bit 6, so
    // the legacy AllDirty path spuriously appends primvars. dirtyMaterial() emits
    // only the material locator; extension-attribute primvars are dirtied separately.
    HdDataSourceLocatorSet expected;
    expected.append(HdMaterialSchema::GetDefaultLocator());
    EXPECT_EQ(expected, NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyMaterial(); }));
}

// ---- Render-item topoChanged: topology locators only ----
//
// Topology changes emit mesh/basisCurves topology locators only. Face-varying primvars
// (st, tangents, normals) are not dirtied on the topology path — render delegates should
// full-rebuild from topology (Pixar guidance: stable-topology vs deforming-geometry distinction).

TEST(FvpDirtyNotifier, renderItemMeshTopoChangeEmitsTopologyOnly)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdMeshSchema::GetSubdivisionSchemeLocator());
    expected.append(HdMeshTopologySchema::GetDefaultLocator());

    EXPECT_EQ(
        expected,
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyMeshTopology(); }));

    HdDataSourceLocatorSet actual = NotifierLocators([](Fvp::FvpDirtyNotifier& n) {
        n.dirtyMeshTopology();
    });
    EXPECT_FALSE(actual.Contains(HdPrimvarsSchema::GetDefaultLocator()));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetNormalsLocator()));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("st"))));
    EXPECT_FALSE(
        actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("tangents"))));
}

TEST(FvpDirtyNotifier, renderItemCurveTopoChangeOmitsMeshPrimvars)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdBasisCurvesTopologySchema::GetDefaultLocator());

    EXPECT_EQ(
        expected,
        NotifierLocators([](Fvp::FvpDirtyNotifier& n) { n.dirtyBasisCurvesTopology(); }));

    HdDataSourceLocatorSet actual = NotifierLocators([](Fvp::FvpDirtyNotifier& n) {
        n.dirtyBasisCurvesTopology();
    });
    EXPECT_FALSE(actual.Intersects(HdMeshTopologySchema::GetDefaultLocator()));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("st"))));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("tangents"))));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetNormalsLocator()));
}

// ---- The render-item geomChanged invalidation shift (rules 1 + 4) ----

TEST(FvpDirtyNotifier, geomChangedEmitsGranularPrimvars)
{
    // geomChanged re-reads all vertex buffers. With useMayaNormals true we
    // emit granular per-primvar locators (points, st, tangents, normals) instead
    // of the broad primvars locator, so the normals skip is real.
    HdDataSourceLocatorSet expected;
    expected.append(HdPrimvarsSchema::GetPointsLocator());
    expected.append(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("st")));
    expected.append(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("tangents")));
    expected.append(HdPrimvarsSchema::GetNormalsLocator());

    EXPECT_EQ(expected, NotifierLocators([](Fvp::FvpDirtyNotifier& n) {
        n.dirtyPoints().dirtyUVs().dirtyTangents().dirtyNormals();
    }));
}

TEST(FvpDirtyNotifier, geomChangedSkipsNormalsWhenHydraGenerates)
{
    // Rule 4: when useMayaNormals is false, dirtyNormals() is not called.
    HdDataSourceLocatorSet expected;
    expected.append(HdPrimvarsSchema::GetPointsLocator());
    expected.append(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("st")));
    expected.append(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("tangents")));

    HdDataSourceLocatorSet actual = NotifierLocators(
        [](Fvp::FvpDirtyNotifier& n) { n.dirtyPoints().dirtyUVs().dirtyTangents(); });
    EXPECT_EQ(expected, actual);

    EXPECT_FALSE(actual.Contains(HdPrimvarsSchema::GetDefaultLocator()))
        << "Broad primvars locator must not be emitted on geomChanged";
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetNormalsLocator()))
        << "Normals locator must be absent when useMayaNormals is false";
}

// ---- Accumulation / dedup / flush behavior ----

TEST(FvpDirtyNotifier, chainingAccumulatesAndDeduplicates)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdXformSchema::GetDefaultLocator());
    expected.append(HdVisibilitySchema::GetDefaultLocator());

    // Calling dirtyTransform() twice must deduplicate.
    EXPECT_EQ(expected, NotifierLocators([](Fvp::FvpDirtyNotifier& n) {
        n.dirtyTransform().dirtyVisibility().dirtyTransform();
    }));
}

TEST(FvpDirtyNotifier, flushClearsPendingLocators)
{
    HdRetainedSceneIndexRefPtr sceneIndex = HdRetainedSceneIndex::New();
    Fvp::FvpDirtyNotifier      notifier(*sceneIndex, kTestPrimPath);
    notifier.dirtyTransform();
    EXPECT_FALSE(notifier.IsEmpty());
    notifier.flush();
    EXPECT_TRUE(notifier.IsEmpty());
    // Second flush is a no-op and must not re-error.
    notifier.flush();
    EXPECT_TRUE(notifier.IsEmpty());
}
