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
// FvpDirtyNotifier accumulates targeted HdDataSourceLocator dirty sets for one prim,
// replacing the legacy HdDirtyBits -> HdDirtyBitsTranslator path. Callers chain
// semantic dirty*() methods and explicitly flush() to notify HdRetainedSceneIndex.
//
#ifndef FVP_DIRTY_NOTIFIER_H
#define FVP_DIRTY_NOTIFIER_H

#include <flowViewport/api.h>

#include <pxr/pxr.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/dataSourceLocator.h>
#include <pxr/usd/sdf/path.h>

PXR_NAMESPACE_OPEN_SCOPE
class HdRetainedSceneIndex;
PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

/// \class FvpDirtyNotifier
///
/// Semantic accumulator of dirty Hydra data-source locators for a single prim.
///
/// FvpDirtyNotifier replaces the opaque HdDirtyBits -> HdDirtyBitsTranslator path
/// with direct, targeted HdDataSourceLocatorSet emission. Each named dirty*()
/// method appends the appropriate locator(s) to an internal, auto-deduplicating
/// HdDataSourceLocatorSet and returns *this for chaining. Nothing is sent to the
/// scene index until flush() is called.
///
/// The API is intentionally GRANULAR: prefer the specific per-primvar locator so
/// the render delegate only re-pulls what actually changed. The broad
/// dirtyPrimvars() locator is reserved for cases where many/unknown primvars
/// genuinely change at once (e.g. connectivity/topology change, extension-attribute
/// add/remove). Every dirty*() call crosses into the render delegate and must
/// correspond to a real change.
///
/// Policy for topology vs deformation (which locators to emit, and how render
/// delegates should interpret them): doc/render_delegate_topology_vs_deformation.md
///
/// Flush is explicit: call flush() to send the accumulated locators to the scene index.
///
/// \note The constructor takes HdRetainedSceneIndex& rather than HdSceneIndexBase&.
/// This is intentional: flush() calls DirtyPrims(), which is the public mutation API
/// for pushing dirty notifications and exists only on HdRetainedSceneIndex, not on
/// HdSceneIndexBase. Callers must therefore hold a concrete HdRetainedSceneIndex
/// (or a class derived from it, such as MayaHydraSceneIndex).
class FvpDirtyNotifier
{
public:
    FVP_API
    FvpDirtyNotifier(PXR_NS::HdRetainedSceneIndex& sceneIndex, const PXR_NS::SdfPath& primPath);

    FVP_API
    ~FvpDirtyNotifier();

    /// Sends DirtyPrims to the scene index for the accumulated locators, then
    /// clears the pending set. Idempotent: a second call with no new dirty*()
    /// calls in between is a no-op.
    FVP_API
    void flush();

    /// Whether any locators are currently pending (i.e. not yet flushed).
    bool IsEmpty() const { return _locators.IsEmpty(); }

    /// Read-only access to the pending locator set (mainly for tests/harnesses).
    const PXR_NS::HdDataSourceLocatorSet& GetLocators() const { return _locators; }

    // ---- Rprim / geometry ----
    FVP_API FvpDirtyNotifier& dirtyTransform();
    FVP_API FvpDirtyNotifier& dirtyVisibility();

    // ---- Primvars: GRANULAR by design ----
    // Prefer the specific per-primvar locator so the render delegate only re-pulls
    // what actually changed. We do NOT emit the broad primvars locator as a
    // catch-all, and we never emit a primvar locator "just in case" - every
    // dirty*() call crosses into the render delegate and must correspond to a
    // real change.
    FVP_API FvpDirtyNotifier& dirtyPoints();        // primvars/points
    FVP_API FvpDirtyNotifier& dirtyNormals();       // primvars/normals - emit ONLY when useMayaNormals
    FVP_API FvpDirtyNotifier& dirtyPrimvar(const PXR_NS::TfToken& name); // primvars/<name>
    FVP_API FvpDirtyNotifier& dirtyUVs();           // dirtyPrimvar("st")
    FVP_API FvpDirtyNotifier& dirtyTangents();      // dirtyPrimvar("tangents")
    // Invalidates the per-object display color (primvars/displayColor, constant interpolation).
    // Used when the render item's single shading/wireframe color changes and no vertex color
    // set is present. See dirtyVertexColors() for the per-vertex case.
    FVP_API FvpDirtyNotifier& dirtyDisplayColor();

    // Invalidates per-vertex color sets (primvars/displayColor, vertex interpolation).
    // When a Maya color set is present it replaces the constant display color entirely —
    // the two modes are mutually exclusive but share the same primvar name and locator,
    // which is why both methods emit primvars/displayColor internally.
    FVP_API FvpDirtyNotifier& dirtyVertexColors();
    FVP_API FvpDirtyNotifier& dirtyPrimvars();      // broad primvars locator - use ONLY when
                                                    // many/unknown primvars change at once.
    // Matches HdDirtyBitsTranslator::RprimDirtyBitsToLocatorSet for DirtyPrimvar on rprims.
    // Sprim DirtyPrimvar does not include this schema; emit only for rprim broad primvar invalidation.
    FVP_API FvpDirtyNotifier& dirtyExtComputationPrimvars();
    FVP_API FvpDirtyNotifier& dirtyTopology();      // mesh > subdivisionScheme + meshTopology
    FVP_API FvpDirtyNotifier& dirtyExtent();        // extent

    /// Canonical locator bundle for rprim connectivity/topology changes on the mesh-adapter
    /// path (mesh, nurbs curve): topology + broad primvars + points + extent. Optionally
    /// includes normals when \p useMayaNormals is true (mesh adapter only).
    /// extComputationPrimvars is intentionally NOT included — skinning/blendshape only.
    /// See doc/render_delegate_topology_vs_deformation.md
    FVP_API static void DirtyRprimConnectivityLocators(FvpDirtyNotifier& notifier, bool useMayaNormals = false);

    /// Locator bundle when Maya mesh displaySmoothMesh or smoothLevel changes (refineLevel
    /// crossing 0): displayStyle + topology + subdivision tags, and normals when
    /// \p useMayaNormals is true. Does NOT emit the broad primvars locator.
    FVP_API static void DirtySmoothMeshDisplayLocators(FvpDirtyNotifier& notifier, bool useMayaNormals = false);

    FVP_API FvpDirtyNotifier& dirtyDoubleSided();   // mesh > doubleSided
    FVP_API FvpDirtyNotifier& dirtyCullStyle();     // displayStyle > cullStyle
    FVP_API FvpDirtyNotifier& dirtySubdivision();   // subdivisionTags
    FVP_API FvpDirtyNotifier& dirtyDisplayStyle();  // displayStyle
    FVP_API FvpDirtyNotifier& dirtyMaterialBinding();

    // ---- Sprim / lights ----
    FVP_API FvpDirtyNotifier& dirtyLightParams();   // light schema only (HdLight::DirtyParams|DirtyShadowParams)
                                                    // Call sites that also need collections or primvars must
                                                    // chain dirtyCollections() / dirtyPrimvars() explicitly.
                                                    // See _dirtyBuiltInLightParams() for the full canonical combination.
    FVP_API FvpDirtyNotifier& dirtyCollections();   // collections (light linking / shadow collection)

    // ---- Sprim / camera ----
    FVP_API FvpDirtyNotifier& dirtyCameraParams();  // camera

    // ---- Sprim / material ----
    FVP_API FvpDirtyNotifier& dirtyMaterial();      // material

    // ---- Instancer ----
    FVP_API FvpDirtyNotifier& dirtyInstancer();     // instancedBy + instancerTopology

private:
    FvpDirtyNotifier& _append(const PXR_NS::HdDataSourceLocator& locator);

    PXR_NS::HdRetainedSceneIndex&  _sceneIndex;
    PXR_NS::SdfPath                _primPath;
    PXR_NS::HdDataSourceLocatorSet _locators;
};

} // namespace FVP_NS_DEF

#endif // FVP_DIRTY_NOTIFIER_H
