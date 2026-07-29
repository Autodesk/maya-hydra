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

#ifndef MAYAHYDRALIB_CUSTOM_DAG_ADAPTER_H
#define MAYAHYDRALIB_CUSTOM_DAG_ADAPTER_H

#include <mayaHydraLib/adapters/dagAdapter.h>

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/dictionary.h>

PXR_NAMESPACE_OPEN_SCOPE

class MayaHydraSceneIndex;

/**
 * \brief Adapter for translating custom Maya plugin nodes to Hydra.
 *
 * This adapter provides a translation mechanism for custom DAG nodes
 * created by third-party Maya plugins (Arnold, RenderMan, etc.) that have no
 * specific registered adapter in maya-hydra. It enables these plugin nodes to
 * participate in the Hydra scene without maya-hydra needing any knowledge of
 * the plugin's node types.
 *
 * \section mechanism Translation Mechanism
 *
 * When MayaHydraSceneIndex::InsertDag() encounters a DAG leaf node that does
 * not match any registered light, camera, or shape adapter, it checks whether
 * the node was registered by a plugin (via MNodeClass::pluginName()). If so,
 * this adapter is created as a fallback. Maya built-in nodes (locators, joints,
 * constraints, etc.) are always skipped.
 *
 * The adapter inserts a Hydra prim of type "mayaCustomDagNode" into the scene
 * index. This prim carries:
 *   - Standard xform, visibility, and purpose data sources (from MayaHydraDagAdapter).
 *   - A custom "mayaNode" data source (MayaHydraCustomNodeDataSource) containing:
 *     - "mayaTypeName": the Maya node type name (e.g. "aiPhotometricLight").
 *     - "mayaAttributes": a VtDictionary of all non-default attribute values.
 *
 * \section sceneidx Render Delegate Scene Index
 *
 * The adapter intentionally makes no assumptions about what the custom node
 * represents (light, mesh, camera, volume, etc.). The render delegate is
 * responsible for providing its own HdSceneIndexPlugin (registered via
 * HdSceneIndexPluginRegistry::RegisterSceneIndexForRenderer) that:
 *   1. Observes mayaCustomDagNode prims in the scene.
 *   2. Reads the mayaNode data source to identify the Maya type and attributes.
 *   3. Re-types the prim and maps attributes to what the render delegate expects.
 *   4. Ignores types it does not recognize (passes them through).
 *
 * This design keeps the render delegate free of any maya-hydra dependency.
 * See flowViewportAPIExamples/customNodeTranslationSceneIndex/ for a complete
 * example that translates aiPhotometricLight to a UsdLux sphereLight for Arnold.
 *
 * \section defaults Attribute Filtering
 *
 * Standard Maya built-in base class attributes (from locator, shape,
 * dagNode, dependNode, etc.) are excluded because maya-hydra already
 * handles them through dedicated xform, visibility, and purpose data
 * sources. Message attributes are also excluded. Among the remaining
 * plugin-specific attributes, only those whose values differ from their
 * registered defaults are included in the mayaAttributes dictionary.
 *
 * The adapter is always created for plugin nodes regardless of whether
 * any attributes are currently non-default. This ensures nodes created
 * with default values are still tracked; when the user later modifies an
 * attribute, the existing attribute-changed callback fires a dirty notice
 * and the data source returns the updated non-default values.
 *
 * \section dirty Per-Attribute Dirty Locators
 *
 * When a Maya attribute changes, the adapter fires a dirty notice with the
 * locator "mayaNode.mayaAttributes.<attrName>". This allows the render
 * delegate's scene index to react to specific attribute changes without
 * recomputing everything.
 */
class MayaHydraCustomDagAdapter : public MayaHydraDagAdapter
{
public:
    MAYAHYDRALIB_API
    MayaHydraCustomDagAdapter(
        MayaHydraSceneIndex* mayaHydraSceneIndex,
        const MDagPath& dagPath);

    MAYAHYDRALIB_API
    ~MayaHydraCustomDagAdapter() override = default;

    /// Always returns true for plugin nodes. The adapter is created for all
    /// plugin DAG nodes so that attribute changes are tracked from the start.
    MAYAHYDRALIB_API
    bool IsSupported() const override;

    /// Inserts the prim into the Hydra scene index as a mayaCustomDagNode.
    MAYAHYDRALIB_API
    void Populate() override;

    /// Registers Maya callbacks for this adapter. Unlike the base class which
    /// registers a transform-dirty callback on every node in the DAG path
    /// (including the shape), this override separates concerns:
    ///   - Shape node: attribute-changed callback for per-attribute dirty
    ///     locators, plus a visibility-only plug-dirty callback.
    ///   - Parent transforms: standard transform-dirty callback for xform
    ///     and visibility changes.
    /// This prevents spurious xform dirty notices when plugin attributes
    /// (e.g. intensity) change on the shape node.
    MAYAHYDRALIB_API
    void CreateCallbacks() override;

    /// Removes the prim from the Hydra scene index.
    MAYAHYDRALIB_API
    void RemovePrim() override;

    /// Queries visibility directly from the DAG path.
    MAYAHYDRALIB_API
    bool GetVisible() override;

    /// Returns the Maya node type name (e.g. "aiPhotometricLight").
    const TfToken& GetMayaTypeName() const { return _mayaTypeName; }

    /// Reads all non-default attributes from the Maya node into a VtDictionary.
    MAYAHYDRALIB_API
    VtDictionary GetNonDefaultMayaAttributes() const;

private:
    TfToken _mayaTypeName;  ///< Cached Maya node type name.
};

using MayaHydraCustomDagAdapterPtr = std::shared_ptr<MayaHydraCustomDagAdapter>;

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_CUSTOM_DAG_ADAPTER_H
