//
// Copyright 2019 Luma Pictures
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
#include "shapeAdapter.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/mayaUtils.h>

#include <pxr/base/tf/type.h>

#include <maya/MFnDagNode.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraShapeAdapter, TfType::Bases<MayaHydraDagAdapter>>();
}

/*
 * MayaHydraShapeAdapter is an adapter to translate from Maya shapes to hydra
 * Please note that, at this time, this is not used by the hydra plug-in, we translate from a
 * renderitem to hydra using the MayaHydraRenderItemAdapter class.
 */
MayaHydraShapeAdapter::MayaHydraShapeAdapter(
    const SdfPath&        id,
    MayaHydraSceneIndex* mayaHydraSceneIndex,
    const MDagPath&       dagPath)
    : MayaHydraDagAdapter(id, mayaHydraSceneIndex, dagPath)
{
}

size_t MayaHydraShapeAdapter::SamplePrimvar(
    const TfToken& key,
    size_t         maxSampleCount,
    float*         times,
    VtValue*       samples)
{
    if (maxSampleCount < 1) {
        return 0;
    }
    times[0] = 0.0f;
    samples[0] = Get(key);
    return 1;
}

HdMeshTopology MayaHydraShapeAdapter::GetMeshTopology() { return {}; };

HdBasisCurvesTopology MayaHydraShapeAdapter::GetBasisCurvesTopology() { return {}; };

HdDisplayStyle MayaHydraShapeAdapter::GetDisplayStyle() { return { 0, false, false }; }

PxOsdSubdivTags MayaHydraShapeAdapter::GetSubdivTags() { return {}; }

MObject MayaHydraShapeAdapter::GetMaterial(const MObject& shadingComp)
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg(
            "Called MayaHydraShapeAdapter::GetMaterial() - %s\n",
            GetDagPath().partialPathName().asChar());

    return MayaHydra::FindShadingEngine(GetDagPath(), shadingComp);
}

GfBBox3d MayaHydraShapeAdapter::GetBoundingBox()
{
    MFnDagNode   node(GetDagPath());
    MBoundingBox objBB = node.boundingBox();
    MPoint       minPt = objBB.min();
    MPoint       maxPt = objBB.max();
    GfRange3d range(GfVec3d(minPt.x, minPt.y, minPt.z), GfVec3d(maxPt.x, maxPt.y, maxPt.z));
    GfBBox3d  bbox = GfBBox3d(range, GetTransform());

    return bbox;
}

PXR_NAMESPACE_CLOSE_SCOPE
