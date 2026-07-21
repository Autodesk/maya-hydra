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
// DirtyNotifier accumulates targeted HdDataSourceLocator dirty sets for one prim,
// replacing the legacy HdDirtyBits -> HdDirtyBitsTranslator path. Callers chain
// semantic dirty*() methods; pending locators are flushed automatically on destruction.
// flush() remains available for early emission and is idempotent.
//
#ifndef FVP_DIRTY_NOTIFIER_H
#define FVP_DIRTY_NOTIFIER_H

#include <flowViewport/api.h>

#include <pxr/pxr.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/weakPtr.h>
#include <pxr/imaging/hd/dataSourceLocator.h>
#include <pxr/usd/sdf/path.h>

PXR_NAMESPACE_OPEN_SCOPE
class HdRetainedSceneIndex;
PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

/// \class DirtyNotifier
///
/// Semantic accumulator of dirty Hydra data-source locators for a single prim.
///
/// DirtyNotifier replaces the opaque HdDirtyBits -> HdDirtyBitsTranslator path
/// with direct, targeted HdDataSourceLocatorSet emission. Each named dirty*()
/// method appends the appropriate locator(s) to an internal, auto-deduplicating
/// HdDataSourceLocatorSet and returns *this for chaining. Pending locators are flushed
/// automatically when the notifier is destroyed (RAII). Call flush() explicitly only
/// when dirty notifications must be sent before the notifier goes out of scope.
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
/// Typical usage is a single chained expression at end of scope; explicit flush() is
/// optional and harmless if called twice.
///
/// \note The constructor takes HdRetainedSceneIndex& rather than HdSceneIndexBase&.
/// This is intentional: flush() calls DirtyPrims(), which is the public mutation API
/// for pushing dirty notifications and exists only on HdRetainedSceneIndex, not on
/// HdSceneIndexBase. Callers must therefore hold a concrete HdRetainedSceneIndex
/// (or a class derived from it, such as MayaHydraSceneIndex).
///
/// \note Lifetime: \p sceneIndex must outlive this DirtyNotifier. The notifier stores
/// a non-owning TfWeakPtr to the scene index and calls DirtyPrims() on flush(). If the
/// scene index is destroyed while locators are still pending, flush() emits
/// TF_CODING_ERROR and drops the pending set rather than dereferencing a stale object.
class DirtyNotifier
{
public:
    /// \p sceneIndex must remain valid until pending locators have been flushed
    /// (explicitly or when this notifier is destroyed).
    FVP_API
    DirtyNotifier(PXR_NS::HdRetainedSceneIndex& sceneIndex, const PXR_NS::SdfPath& primPath);

    FVP_API
    ~DirtyNotifier();

    /// Sends DirtyPrims to the scene index for the accumulated locators, then
    /// clears the pending set. Idempotent: a second call with no new dirty*()
    /// calls in between is a no-op. If the scene index was destroyed, emits
    /// TF_CODING_ERROR and clears pending locators without calling DirtyPrims().
    FVP_API
    void flush();

    /// Whether any locators are currently pending (i.e. not yet flushed).
    bool IsEmpty() const { return _locators.IsEmpty(); }

    /// Read-only access to the pending locator set (mainly for tests/harnesses).
    const PXR_NS::HdDataSourceLocatorSet& GetLocators() const { return _locators; }

    // ---- Rprim / geometry ----
    FVP_API DirtyNotifier& dirtyTransform();
    FVP_API DirtyNotifier& dirtyVisibility();

    // ---- Primvars: GRANULAR by design ----
    // Prefer the specific per-primvar locator so the render delegate only re-pulls
    // what actually changed. We do NOT emit the broad primvars locator as a
    // catch-all, and we never emit a primvar locator "just in case" - every
    // dirty*() call crosses into the render delegate and must correspond to a
    // real change.
    FVP_API DirtyNotifier& dirtyPoints();        // primvars/points
    FVP_API DirtyNotifier& dirtyNormals();       // primvars/normals - emit ONLY when useMayaNormals
    FVP_API DirtyNotifier& dirtyPrimvar(const PXR_NS::TfToken& name); // primvars/<name>
    // Invalidates the per-object display color (primvars/displayColor, constant interpolation).
    // Used when the render item's single shading/wireframe color changes and no vertex color
    // set is present. See dirtyVertexColors() for the per-vertex case.
    FVP_API DirtyNotifier& dirtyDisplayColor();

    // Invalidates per-vertex color sets (primvars/displayColor, vertex interpolation).
    // When a Maya color set is present it replaces the constant display color entirely —
    // the two modes are mutually exclusive but share the same primvar name and locator,
    // which is why both methods emit primvars/displayColor internally.
    FVP_API DirtyNotifier& dirtyVertexColors();
    FVP_API DirtyNotifier& dirtyPrimvars();      // broad primvars locator - use ONLY when
                                                    // many/unknown primvars change at once.
    // Matches HdDirtyBitsTranslator::RprimDirtyBitsToLocatorSet for DirtyPrimvar on rprims.
    // Sprim DirtyPrimvar does not include this schema; emit only for rprim broad primvar invalidation.
    FVP_API DirtyNotifier& dirtyExtComputationPrimvars();
    /// Mesh-only: subdivisionScheme + meshTopology.
    FVP_API DirtyNotifier& dirtyMeshTopology();
    /// BasisCurves-only: basisCurves/topology.
    FVP_API DirtyNotifier& dirtyBasisCurvesTopology();
    /// Dispatches to dirtyMeshTopology() or dirtyBasisCurvesTopology() per \p primType.
    /// Points and other rprim types have no topology schema locators; emits a warning and no-op.
    FVP_API DirtyNotifier& dirtyTopology(const PXR_NS::TfToken& primType);
    FVP_API DirtyNotifier& dirtyExtent();        // extent

    /// Canonical locator bundle for rprim connectivity/topology changes: topology +
    /// broad primvars + points + extent. Emits mesh or basisCurves topology locators per
    /// \p primType; warns and no-ops for unsupported rprim types (e.g. points). Does not
    /// emit granular UV/tangent/normal locators — topology (or broad
    /// primvars on the mesh-adapter path) is sufficient for render delegates to full-rebuild.
    /// extComputationPrimvars is intentionally NOT included — skinning/blendshape only.
    /// See doc/render_delegate_topology_vs_deformation.md
    FVP_API static void DirtyRprimConnectivityLocators(
        DirtyNotifier& notifier,
        const PXR_NS::TfToken& primType);

    /// Locator bundle when Maya mesh displaySmoothMesh or smoothLevel changes (refineLevel
    /// crossing 0): displayStyle + topology + subdivision tags. Does NOT emit the broad
    /// primvars locator or granular face-varying primvar locators.
    FVP_API static void DirtySmoothMeshDisplayLocators(DirtyNotifier& notifier);

    FVP_API DirtyNotifier& dirtyDoubleSided();   // mesh > doubleSided
    FVP_API DirtyNotifier& dirtyCullStyle();     // displayStyle > cullStyle
    FVP_API DirtyNotifier& dirtySubdivision();   // subdivisionTags
    FVP_API DirtyNotifier& dirtyDisplayStyle();  // displayStyle
    FVP_API DirtyNotifier& dirtyMaterialBinding();

    // ---- Sprim / lights ----
    FVP_API DirtyNotifier& dirtyLightParams();   // light schema only (HdLight::DirtyParams|DirtyShadowParams)
                                                    // Call sites that also need collections or primvars must
                                                    // chain dirtyCollections() / dirtyPrimvars() explicitly.
                                                    // See _dirtyBuiltInLightParams() for the full canonical combination.
    FVP_API DirtyNotifier& dirtyCollections();   // collections (light linking / shadow collection)

    // ---- Sprim / camera ----
    FVP_API DirtyNotifier& dirtyCameraParams();  // camera

    // ---- Sprim / material ----
    FVP_API DirtyNotifier& dirtyMaterial();      // material

    // ---- Instancer ----
    FVP_API DirtyNotifier& dirtyInstancer();     // instancedBy + instancerTopology

    // Batching is used to coalesce multiple DirtyPrims notifications.
    FVP_API static void beginDirtyBatch(PXR_NS::HdRetainedSceneIndex& batchingSceneIndex);
    FVP_API static void commitDirtyBatch();

private:
    DirtyNotifier& _append(const PXR_NS::HdDataSourceLocator& locator);

    const PXR_NS::TfWeakPtr<PXR_NS::HdRetainedSceneIndex> _sceneIndex;
    const PXR_NS::SdfPath                                 _primPath;
    PXR_NS::HdDataSourceLocatorSet _locators;
};

} // namespace FVP_NS_DEF

#endif // FVP_DIRTY_NOTIFIER_H
