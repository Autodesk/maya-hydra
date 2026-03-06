//
// Copyright 2025 Autodesk, Inc. All rights reserved.
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

#include "simpleHydraGenerativeProcedural.h"

#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/materialBindingSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/vt/array.h>

PXR_NAMESPACE_OPEN_SCOPE

const SdfPath SimpleHydraGenerativeProcedural::_cube0Path("/cube0");
const SdfPath SimpleHydraGenerativeProcedural::_cube1Path("/cube1");

const TfToken SimpleHydraGenerativeProcedural::_cube0Name("cube0");
const TfToken SimpleHydraGenerativeProcedural::_cube1Name("cube1");

SimpleHydraGenerativeProcedural::SimpleHydraGenerativeProcedural(
    const SdfPath &proceduralPrimPath)
    : HdGpGenerativeProcedural(proceduralPrimPath)
{}

SimpleHydraGenerativeProcedural::~SimpleHydraGenerativeProcedural() = default;

SimpleHydraGenerativeProcedural::DependencyMap
SimpleHydraGenerativeProcedural::UpdateDependencies(
    const HdSceneIndexBaseRefPtr &)
{
    DependencyMap result;
    result[_GetProceduralPrimPath()] = HdPrimvarsSchema::GetDefaultLocator();
    return result;
}

SimpleHydraGenerativeProcedural::ChildPrimTypeMap
SimpleHydraGenerativeProcedural::Update(
    const HdSceneIndexBaseRefPtr &,
    const ChildPrimTypeMap &previousResult,
    const DependencyMap &,
    HdSceneIndexObserver::DirtiedPrimEntries *)
{
    ChildPrimTypeMap result;
    const SdfPath &base = _GetProceduralPrimPath();
    result[base.AppendChild(_cube0Name)] = HdPrimTypeTokens->mesh;
    result[base.AppendChild(_cube1Name)] = HdPrimTypeTokens->mesh;
    return result;
}

HdSceneIndexPrim
SimpleHydraGenerativeProcedural::GetChildPrim(
    const HdSceneIndexBaseRefPtr &inputScene,
    const SdfPath &childPrimPath)
{
    // Read materialPath primvar from the procedural prim
    SdfPath materialPath;
    HdSceneIndexPrim procPrim = inputScene->GetPrim(_GetProceduralPrimPath());
    if (procPrim.dataSource) {
        HdPrimvarSchema matPathPrimvar =
            HdPrimvarsSchema::GetFromParent(procPrim.dataSource)
                             .GetPrimvar(TfToken("materialPath"));
        if (matPathPrimvar) {
            if (HdSampledDataSourceHandle valueDs = matPathPrimvar.GetPrimvarValue()) {
                VtValue v = valueDs->GetValue(0.0f);
                if (v.IsHolding<TfToken>()) {
                    materialPath = _GetProceduralPrimPath().AppendPath(
                        SdfPath(v.UncheckedGet<TfToken>().GetString()));
                }
            }
        }
    }

    const SdfPath &base = _GetProceduralPrimPath();
    if (childPrimPath == base.AppendChild(_cube0Name)) {
        GfMatrix4d xform; xform.SetTranslate(GfVec3d(-3.0, 0.0, 0.0));
        return _BuildCubePrim(1.0f, xform, materialPath);
    }
    if (childPrimPath == base.AppendChild(_cube1Name)) {
        GfMatrix4d xform; xform.SetTranslate(GfVec3d(3.0, 0.0, 0.0));
        return _BuildCubePrim(1.0f, xform, materialPath);
    }
    return { TfToken(), nullptr };
}

HdSceneIndexPrim
SimpleHydraGenerativeProcedural::_BuildCubePrim(
    float halfSize,
    const GfMatrix4d &transform,
    const SdfPath &materialPath) const
{
    using _PointArrayDs = HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>;
    using _IntArrayDs   = HdRetainedTypedSampledDataSource<VtIntArray>;

    static const VtIntArray faceVertexCounts  = {4, 4, 4, 4, 4, 4};
    static const VtIntArray faceVertexIndices = {0,1,3,2, 2,3,5,4, 4,5,7,6,
                                                  6,7,1,0, 1,7,5,3, 6,0,2,4};

    const VtArray<GfVec3f> points = {
        {-halfSize, -halfSize,  halfSize},
        { halfSize, -halfSize,  halfSize},
        {-halfSize,  halfSize,  halfSize},
        { halfSize,  halfSize,  halfSize},
        {-halfSize,  halfSize, -halfSize},
        { halfSize,  halfSize, -halfSize},
        {-halfSize, -halfSize, -halfSize},
        { halfSize, -halfSize, -halfSize},
    };

    const HdContainerDataSourceHandle meshDs =
        HdMeshSchema::Builder()
            .SetTopology(HdMeshTopologySchema::Builder()
                .SetFaceVertexCounts(_IntArrayDs::New(faceVertexCounts))
                .SetFaceVertexIndices(_IntArrayDs::New(faceVertexIndices))
                .Build())
            .Build();

    const HdContainerDataSourceHandle primvarsDs =
        HdRetainedContainerDataSource::New(
            HdPrimvarsSchemaTokens->points,
            HdPrimvarSchema::Builder()
                .SetPrimvarValue(_PointArrayDs::New(points))
                .SetInterpolation(HdPrimvarSchema::BuildInterpolationDataSource(
                    HdPrimvarSchemaTokens->vertex))
                .SetRole(HdPrimvarSchema::BuildRoleDataSource(
                    HdPrimvarSchemaTokens->point))
                .Build()
        );

    static const TfToken purposes[] = { HdMaterialBindingsSchemaTokens->allPurpose };
    HdDataSourceBaseHandle const materialBindingSources[] = {
        HdMaterialBindingSchema::Builder()
            .SetPath(HdRetainedTypedSampledDataSource<SdfPath>::New(materialPath))
            .Build()
    };

    const GfRange3d extent({-halfSize,-halfSize,-halfSize},
                           { halfSize, halfSize, halfSize});

    HdSceneIndexPrim prim;
    prim.primType = HdPrimTypeTokens->mesh;
    prim.dataSource = HdRetainedContainerDataSource::New(
        HdXformSchemaTokens->xform,
        HdXformSchema::Builder()
            .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(transform))
            .Build(),
        HdExtentSchemaTokens->extent,
        HdExtentSchema::Builder()
            .SetMin(HdRetainedTypedSampledDataSource<GfVec3d>::New(extent.GetMin()))
            .SetMax(HdRetainedTypedSampledDataSource<GfVec3d>::New(extent.GetMax()))
            .Build(),
        HdMeshSchemaTokens->mesh,        meshDs,
        HdPrimvarsSchemaTokens->primvars, primvarsDs,
        HdMaterialBindingsSchemaTokens->materialBindings,
        HdMaterialBindingsSchema::BuildRetained(
            TfArraySize(purposes), purposes, materialBindingSources)
    );
    return prim;
}

PXR_NAMESPACE_CLOSE_SCOPE
