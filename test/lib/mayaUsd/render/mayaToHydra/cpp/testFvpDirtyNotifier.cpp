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
// Migration gate for DirtyNotifier: for each representative HdDirtyBits combination,
// compares HdDirtyBitsTranslator output to the equivalent notifier emission. Deliberately
// narrower cases (extComputation primvars, light params) assert explicit expected sets.
// Most cases use Fvp::DirtyNotifier only; geomChanged UV/tangent paths use
// MayaHydra::DirtyNotifier for dirtyUVs()/dirtyTangents().

#include <gtest/gtest.h>

#include <flowViewport/fvpDirtyNotifier.h>
#include <mayaHydraLib/adapters/mhDirtyNotifier.h>
#include <mayaHydraLib/adapters/tokens.h>

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
#include <memory>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const SdfPath kTestPrimPath("/dirtyNotifierTestPrim");

// Apply a set of dirty*() calls to a fresh notifier and return the accumulated
// locator set. Flushes afterwards so the explicit-flush contract is honored.
HdDataSourceLocatorSet
NotifierLocators(const std::function<void(Fvp::DirtyNotifier&)>& applyDirty)
{
    HdRetainedSceneIndexRefPtr sceneIndex = HdRetainedSceneIndex::New();
    HdDataSourceLocatorSet     result;
    {
        Fvp::DirtyNotifier notifier(*sceneIndex, kTestPrimPath);
        applyDirty(notifier);
        result = notifier.GetLocators();
        notifier.flush();
    }
    return result;
}

HdDataSourceLocatorSet
MhNotifierLocators(const std::function<void(MayaHydra::DirtyNotifier&)>& applyDirty)
{
    HdRetainedSceneIndexRefPtr sceneIndex = HdRetainedSceneIndex::New();
    HdDataSourceLocatorSet     result;
    {
        MayaHydra::DirtyNotifier notifier(*sceneIndex, kTestPrimPath);
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

TEST(DirtyNotifier, transformMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyTransform),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyTransform(); }));
}

TEST(DirtyNotifier, visibilityMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyVisibility),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyVisibility(); }));
}

TEST(DirtyNotifier, doubleSidedMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyDoubleSided),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyDoubleSided(); }));
}

TEST(DirtyNotifier, materialBindingMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyMaterialId),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyMaterialBinding(); }));
}

TEST(DirtyNotifier, meshTopologyMatchesTranslator)
{
    // Rule 2: topology only - no primvars, no extent.
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyTopology),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyMeshTopology(); }));
}

TEST(DirtyNotifier, basisCurvesTopologyMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->basisCurves, HdChangeTracker::DirtyTopology),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyBasisCurvesTopology(); }));
}

TEST(DirtyNotifier, dirtyTopologyDispatchesByPrimType)
{
    EXPECT_EQ(
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyMeshTopology(); }),
        NotifierLocators(
            [](Fvp::DirtyNotifier& n) { n.dirtyTopology(HdPrimTypeTokens->mesh); }));
    EXPECT_EQ(
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyBasisCurvesTopology(); }),
        NotifierLocators(
            [](Fvp::DirtyNotifier& n) { n.dirtyTopology(HdPrimTypeTokens->basisCurves); }));
}

TEST(DirtyNotifier, rprimConnectivityChangeOmitsNormalsByDefault)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdMeshSchema::GetSubdivisionSchemeLocator());
    expected.append(HdMeshTopologySchema::GetDefaultLocator());
    expected.append(HdPrimvarsSchema::GetDefaultLocator());
    expected.append(HdPrimvarsSchema::GetPointsLocator());
    expected.append(HdExtentSchema::GetDefaultLocator());

    EXPECT_EQ(
        expected,
        NotifierLocators([](Fvp::DirtyNotifier& n) {
            Fvp::DirtyNotifier::DirtyRprimConnectivityLocators(n, HdPrimTypeTokens->mesh);
        }));
}

TEST(DirtyNotifier, rprimConnectivityChangeOmitsGranularFaceVaryingPrimvars)
{
    HdDataSourceLocatorSet actual = NotifierLocators([](Fvp::DirtyNotifier& n) {
        Fvp::DirtyNotifier::DirtyRprimConnectivityLocators(n, HdPrimTypeTokens->mesh);
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

TEST(DirtyNotifier, basisCurvesConnectivityChangeOmitsMeshTopology)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdBasisCurvesTopologySchema::GetDefaultLocator());
    expected.append(HdPrimvarsSchema::GetDefaultLocator());
    expected.append(HdPrimvarsSchema::GetPointsLocator());
    expected.append(HdExtentSchema::GetDefaultLocator());

    EXPECT_EQ(
        expected,
        NotifierLocators([](Fvp::DirtyNotifier& n) {
            Fvp::DirtyNotifier::DirtyRprimConnectivityLocators(
                n, HdPrimTypeTokens->basisCurves);
        }));
}

TEST(DirtyNotifier, rprimConnectivityChangeUnsupportedPrimTypeNoOps)
{
    EXPECT_TRUE(NotifierLocators([](Fvp::DirtyNotifier& n) {
        Fvp::DirtyNotifier::DirtyRprimConnectivityLocators(n, HdPrimTypeTokens->points);
    }).IsEmpty());
}

TEST(DirtyNotifier, smoothMeshDisplayOmitsNormalsByDefault)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdLegacyDisplayStyleSchema::GetDefaultLocator());
    expected.append(HdMeshSchema::GetSubdivisionSchemeLocator());
    expected.append(HdMeshTopologySchema::GetDefaultLocator());
    expected.append(HdSubdivisionTagsSchema::GetDefaultLocator());

    EXPECT_EQ(
        expected,
        NotifierLocators([](Fvp::DirtyNotifier& n) {
            Fvp::DirtyNotifier::DirtySmoothMeshDisplayLocators(n);
        }));
}

TEST(DirtyNotifier, smoothMeshDisplayOmitsGranularFaceVaryingPrimvars)
{
    HdDataSourceLocatorSet actual = NotifierLocators([](Fvp::DirtyNotifier& n) {
        Fvp::DirtyNotifier::DirtySmoothMeshDisplayLocators(n);
    });
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetNormalsLocator()));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("st"))));
    EXPECT_FALSE(
        actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("tangents"))));
}

TEST(DirtyNotifier, extentMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyExtent),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyExtent(); }));
}

TEST(DirtyNotifier, cullStyleMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyCullStyle),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyCullStyle(); }));
}

TEST(DirtyNotifier, displayStyleMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyDisplayStyle),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyDisplayStyle(); }));
}

TEST(DirtyNotifier, subdivisionMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtySubdivTags),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtySubdivision(); }));
}

TEST(DirtyNotifier, pointsMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyPoints),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyPoints(); }));
}

TEST(DirtyNotifier, normalsMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorRprimLocators(HdPrimTypeTokens->mesh, HdChangeTracker::DirtyNormals),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyNormals(); }));
}

TEST(DirtyNotifier, cameraParamsMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorSprimLocators(HdPrimTypeTokens->camera, HdCamera::DirtyParams),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyCameraParams(); }));
}

TEST(DirtyNotifier, cameraTransformMatchesTranslator)
{
    EXPECT_EQ(
        TranslatorSprimLocators(HdPrimTypeTokens->camera, HdCamera::DirtyTransform),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyTransform(); }));
}

TEST(DirtyNotifier, materialMatchesTranslator)
{
    // HdMaterial::DirtyVolume and HdChangeTracker::DirtyPrimvar share bit 6, so
    // AllDirty would make TranslatorSprimLocators also append primvars. Any other
    // material dirty bit maps to the material schema locator only.
    EXPECT_EQ(
        TranslatorSprimLocators(HdPrimTypeTokens->material, HdMaterial::DirtyParams),
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyMaterial(); }));
}

TEST(DirtyNotifier, instancerMatchesTranslator)
{
    HdDataSourceLocatorSet expected;
    HdDirtyBitsTranslator::InstancerDirtyBitsToLocatorSet(
        TfToken(),
        HdChangeTracker::DirtyInstancer | HdChangeTracker::DirtyInstanceIndex,
        &expected);
    EXPECT_EQ(expected, NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyInstancer(); }));
}

// ---- Cases where the notifier is deliberately narrower than the translator ----

TEST(DirtyNotifier, primvarsOmitsExtComputation)
{
    // The translator also emits HdExtComputationPrimvarsSchema for DirtyPrimvar,
    // but the Maya adapters do not use ext-computation primvars. The broad
    // dirtyPrimvars() must emit only the primvars locator.
    HdDataSourceLocatorSet expected;
    expected.append(HdPrimvarsSchema::GetDefaultLocator());
    EXPECT_EQ(expected, NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyPrimvars(); }));
}

TEST(DirtyNotifier, lightParamsIsGranular)
{
    // dirtyLightParams() intentionally emits ONLY the light schema locator. The
    // translator additionally emits primvars/visibility/collections for
    // DirtyParams; those are the responsibility of the call site.
    HdDataSourceLocatorSet expected;
    expected.append(HdLightSchema::GetDefaultLocator());
    EXPECT_EQ(expected, NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyLightParams(); }));
}

TEST(DirtyNotifier, materialSkipsPrimvarsOnAllDirty)
{
    // HdMaterial::DirtyVolume and HdChangeTracker::DirtyPrimvar share bit 6, so
    // the legacy AllDirty path spuriously appends primvars. dirtyMaterial() emits
    // only the material locator; extension-attribute primvars are dirtied separately.
    HdDataSourceLocatorSet expected;
    expected.append(HdMaterialSchema::GetDefaultLocator());
    EXPECT_EQ(expected, NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyMaterial(); }));
}

// ---- Render-item topoChanged: topology locators only ----
//
// Topology changes emit mesh/basisCurves topology locators only. Face-varying primvars
// (st, tangents, normals) are not dirtied on the topology path — render delegates should
// full-rebuild from topology (Pixar guidance: stable-topology vs deforming-geometry distinction).

TEST(DirtyNotifier, renderItemMeshTopoChangeEmitsTopologyOnly)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdMeshSchema::GetSubdivisionSchemeLocator());
    expected.append(HdMeshTopologySchema::GetDefaultLocator());

    EXPECT_EQ(
        expected,
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyMeshTopology(); }));

    HdDataSourceLocatorSet actual = NotifierLocators([](Fvp::DirtyNotifier& n) {
        n.dirtyMeshTopology();
    });
    EXPECT_FALSE(actual.Contains(HdPrimvarsSchema::GetDefaultLocator()));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetNormalsLocator()));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("st"))));
    EXPECT_FALSE(
        actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("tangents"))));
}

TEST(DirtyNotifier, renderItemCurveTopoChangeOmitsMeshPrimvars)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdBasisCurvesTopologySchema::GetDefaultLocator());

    EXPECT_EQ(
        expected,
        NotifierLocators([](Fvp::DirtyNotifier& n) { n.dirtyBasisCurvesTopology(); }));

    HdDataSourceLocatorSet actual = NotifierLocators([](Fvp::DirtyNotifier& n) {
        n.dirtyBasisCurvesTopology();
    });
    EXPECT_FALSE(actual.Intersects(HdMeshTopologySchema::GetDefaultLocator()));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("st"))));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetDefaultLocator().Append(TfToken("tangents"))));
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetNormalsLocator()));
}

// ---- The render-item geomChanged invalidation shift (rules 1 + 4) ----

TEST(DirtyNotifier, geomChangedEmitsGranularPrimvars)
{
    // geomChanged re-reads all vertex buffers. With useMayaNormals true we
    // emit granular per-primvar locators (points, st, tangents, normals) instead
    // of the broad primvars locator, so the normals skip is real.
    HdDataSourceLocatorSet expected;
    expected.append(HdPrimvarsSchema::GetPointsLocator());
    expected.append(HdPrimvarsSchema::GetDefaultLocator().Append(MayaHydraAdapterTokens->st));
    expected.append(HdPrimvarsSchema::GetDefaultLocator().Append(MayaHydraAdapterTokens->tangents));
    expected.append(HdPrimvarsSchema::GetNormalsLocator());

    EXPECT_EQ(expected, MhNotifierLocators([](MayaHydra::DirtyNotifier& n) {
        n.dirtyPoints();
        n.dirtyUVs().dirtyTangents().dirtyNormals();
    }));
}

TEST(DirtyNotifier, geomChangedSkipsNormalsWhenHydraGenerates)
{
    // Rule 4: when useMayaNormals is false, dirtyNormals() is not called.
    HdDataSourceLocatorSet expected;
    expected.append(HdPrimvarsSchema::GetPointsLocator());
    expected.append(HdPrimvarsSchema::GetDefaultLocator().Append(MayaHydraAdapterTokens->st));
    expected.append(HdPrimvarsSchema::GetDefaultLocator().Append(MayaHydraAdapterTokens->tangents));

    HdDataSourceLocatorSet actual = MhNotifierLocators([](MayaHydra::DirtyNotifier& n) {
        n.dirtyPoints();
        n.dirtyUVs().dirtyTangents();
    });
    EXPECT_EQ(expected, actual);

    EXPECT_FALSE(actual.Contains(HdPrimvarsSchema::GetDefaultLocator()))
        << "Broad primvars locator must not be emitted on geomChanged";
    EXPECT_FALSE(actual.Intersects(HdPrimvarsSchema::GetNormalsLocator()))
        << "Normals locator must be absent when useMayaNormals is false";
}

// ---- Accumulation / dedup / flush behavior ----

TEST(DirtyNotifier, chainingAccumulatesAndDeduplicates)
{
    HdDataSourceLocatorSet expected;
    expected.append(HdXformSchema::GetDefaultLocator());
    expected.append(HdVisibilitySchema::GetDefaultLocator());

    // Calling dirtyTransform() twice must deduplicate.
    EXPECT_EQ(expected, NotifierLocators([](Fvp::DirtyNotifier& n) {
        n.dirtyTransform().dirtyVisibility().dirtyTransform();
    }));
}

TEST(DirtyNotifier, flushClearsPendingLocators)
{
    HdRetainedSceneIndexRefPtr sceneIndex = HdRetainedSceneIndex::New();
    Fvp::DirtyNotifier      notifier(*sceneIndex, kTestPrimPath);
    notifier.dirtyTransform();
    EXPECT_FALSE(notifier.IsEmpty());
    notifier.flush();
    EXPECT_TRUE(notifier.IsEmpty());
    // Second flush is a no-op and must not re-error.
    notifier.flush();
    EXPECT_TRUE(notifier.IsEmpty());
}

TEST(DirtyNotifier, flushAfterSceneIndexDestroyedDropsLocators)
{
    HdRetainedSceneIndexRefPtr sceneIndex = HdRetainedSceneIndex::New();
    auto                       notifier = std::make_unique<Fvp::DirtyNotifier>(*sceneIndex, kTestPrimPath);
    notifier->dirtyTransform();
    EXPECT_FALSE(notifier->IsEmpty());
    sceneIndex.Reset();
    notifier->flush();
    EXPECT_TRUE(notifier->IsEmpty());
}
