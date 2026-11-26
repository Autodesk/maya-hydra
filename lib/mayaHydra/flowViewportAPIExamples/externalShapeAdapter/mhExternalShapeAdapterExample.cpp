//
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

// Example of a shape adapter that lives outside mayaHydraLib: it translates the
// MhExternalShapeAdapterExample Maya node (mhExternalShapeAdapterExampleNode.py)
// to a Hydra mesh, a blue square.
//
// Nothing loads this library but MayaHydraAdapterRegistry::LoadAllPlugin(), which
// finds it through the plugInfo.json next to it. See the README.

#include <mayaHydraLib/adapters/adapterRegistry.h>
#include <mayaHydraLib/adapters/shapeAdapter.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/pxr.h>

#include <pxr/base/gf/bbox3d.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/type.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/pxOsd/tokens.h>

#include <maya/MDagPath.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    // The Maya node type this adapter translates.
    (MhExternalShapeAdapterExample)
);
// clang-format on

// Half the side of the square.
constexpr float kHalfSize = 2.0f;

} // namespace

/**
 * \brief MhExternalShapeAdapterExample translates a MhExternalShapeAdapterExample Maya node to a
 * Hydra mesh prim: a square in the node's XZ plane, facing +Y.
 */
class MhExternalShapeAdapterExample : public MayaHydraShapeAdapter
{
public:
    MhExternalShapeAdapterExample(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag)
        : MayaHydraShapeAdapter(mayaHydraSceneIndex->GetPrimPath(dag, false), mayaHydraSceneIndex, dag)
    {
    }

    bool IsSupported() const override
    {
        return GetMayaHydraSceneIndex()->IsRprimTypeSupported(HdPrimTypeTokens->mesh);
    }

    void Populate() override
    {
        if (_isPopulated) {
            return;
        }
        GetMayaHydraSceneIndex()->InsertPrim(this, HdPrimTypeTokens->mesh, GetID());
        _isPopulated = true;
    }

    HdMeshTopology GetMeshTopology() override
    {
        // One quad, counter-clockwise seen from +Y.
        return HdMeshTopology(
            PxOsdOpenSubdivTokens->none, HdTokens->rightHanded, VtIntArray { 4 }, VtIntArray { 0, 1, 2, 3 });
    }

    VtValue Get(const TfToken& key) override
    {
        if (key == HdTokens->points) {
            return VtValue(VtVec3fArray { GfVec3f(-kHalfSize, 0.0f, kHalfSize),
                                          GfVec3f(kHalfSize, 0.0f, kHalfSize),
                                          GfVec3f(kHalfSize, 0.0f, -kHalfSize),
                                          GfVec3f(-kHalfSize, 0.0f, -kHalfSize) });
        }
        if (key == HdTokens->displayColor) {
            return VtValue(VtVec3fArray { GfVec3f(0.0f, 0.4f, 1.0f) });
        }
        return {};
    }

    // Only what this prim has: its points, and one colour, as the node has no material.
    HdPrimvarDescriptorVector GetPrimvarDescriptors(HdInterpolation interpolation) override
    {
        if (interpolation == HdInterpolationVertex) {
            return { { HdTokens->points, interpolation, HdPrimvarRoleTokens->point } };
        }
        if (interpolation == HdInterpolationConstant) {
            return { { HdTokens->displayColor, interpolation, HdPrimvarRoleTokens->color } };
        }
        return {};
    }

    // The square's bounds: the Maya node, a locator that draws nothing, has none of its own.
    GfBBox3d GetBoundingBox() override
    {
        return GfBBox3d(
            GfRange3d(GfVec3d(-kHalfSize, 0.0, -kHalfSize), GfVec3d(kHalfSize, 0.0, kHalfSize)),
            GetTransform());
    }
};

// The type, as plugInfo.json declares it: the name and the base have to match.
TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MhExternalShapeAdapterExample, TfType::Bases<MayaHydraShapeAdapter>>();
}

// The adapter's creator, for the Maya node type. It runs when this library is loaded.
TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, MhExternalShapeAdapterExample)
{
    MayaHydraAdapterRegistry::RegisterShapeAdapter(
        _tokens->MhExternalShapeAdapterExample,
        [](MayaHydraSceneIndex* mayaHydraSceneIndex,
           const MDagPath&      dag) -> MayaHydraShapeAdapterPtr {
            return MayaHydraShapeAdapterPtr(
                new MhExternalShapeAdapterExample(mayaHydraSceneIndex, dag));
        });
}

PXR_NAMESPACE_CLOSE_SCOPE
