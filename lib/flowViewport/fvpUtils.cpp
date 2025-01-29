// Copyright 2024 Autodesk
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

#include "fvpUtils.h"

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/dataSourceMaterialNetworkInterface.h>
#include <pxr/imaging/hd/dependenciesSchema.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/smoothNormals.h>
#include <pxr/imaging/hd/vertexAdjacency.h>
#include <pxr/imaging/pxOsd/tokens.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const HdDataSourceLocator pointsValueLocator = HdDataSourceLocator(
    HdPrimvarsSchemaTokens->primvars,
    HdPrimvarsSchemaTokens->points,
    HdPrimvarSchemaTokens->primvarValue);

const HdDataSourceLocator normalsPrimvarLocator = HdDataSourceLocator(
    HdPrimvarsSchemaTokens->primvars,
    HdPrimvarsSchemaTokens->normals);

const HdDataSourceLocator normalsValueLocator = HdDataSourceLocator(
    HdPrimvarsSchemaTokens->primvars,
    HdPrimvarsSchemaTokens->normals,
    HdPrimvarSchemaTokens->primvarValue);

}

namespace FVP_NS_DEF {

#ifdef CODE_COVERAGE_WORKAROUND
void leakSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& si) {
    // Must place the leaked scene index vector on the heap, as a by-value
    // vector will have its destructor called at process exit, which calls
    // the vector element destructors and triggers the crash. 
    static std::vector<PXR_NS::HdSceneIndexBaseRefPtr>* leakedSi{nullptr};
    if (!leakedSi) {
        leakedSi = new std::vector<PXR_NS::HdSceneIndexBaseRefPtr>;
    }
    leakedSi->push_back(si);
}
#endif

PXR_NS::HdDataSourceBaseHandle createSelectionDataSource(const Fvp::PrimSelection& selection)
{
    PXR_NS::HdSelectionSchema::Builder selectionBuilder;
    // Instancers are still expected to be marked "fully selected" even if only certain instances are selected,
    // based on USD's _AddToSelection function in selectionSceneIndexObserver.cpp :
    // https://github.com/PixarAnimationStudios/OpenUSD/blob/f7b8a021ce3d13f91a0211acf8a64a8b780524df/pxr/imaging/hdx/selectionSceneIndexObserver.cpp#L212-L251
    selectionBuilder.SetFullySelected(PXR_NS::HdRetainedTypedSampledDataSource<bool>::New(true));

    std::vector<PXR_NS::HdDataSourceBaseHandle> instanceIndicesDataSources;
    for (const auto& nestedInstanceIndices : selection.nestedInstanceIndices) {
        PXR_NS::HdInstanceIndicesSchema::Builder instanceIndicesBuilder;
        instanceIndicesBuilder.SetInstancer(PXR_NS::HdRetainedTypedSampledDataSource<PXR_NS::SdfPath>::New(nestedInstanceIndices.instancerPath));
        instanceIndicesBuilder.SetPrototypeIndex(PXR_NS::HdRetainedTypedSampledDataSource<int>::New(nestedInstanceIndices.prototypeIndex));
        auto instanceIndices = PXR_NS::VtArray<int>(nestedInstanceIndices.instanceIndices.begin(), nestedInstanceIndices.instanceIndices.end());
        instanceIndicesBuilder.SetInstanceIndices(PXR_NS::HdRetainedTypedSampledDataSource<PXR_NS::VtArray<int>>::New(instanceIndices));
        instanceIndicesDataSources.push_back(PXR_NS::HdDataSourceBase::Cast(instanceIndicesBuilder.Build()));
    }
    if (!instanceIndicesDataSources.empty()) {
        selectionBuilder.SetNestedInstanceIndices(PXR_NS::HdRetainedSmallVectorDataSource::New(instanceIndicesDataSources.size(), instanceIndicesDataSources.data()));
    }
    return PXR_NS::HdDataSourceBase::Cast(selectionBuilder.Build());
}

Fvp::PrimSelection ConvertHydraToFvpSelection(const SdfPath& primPath, const HdSelectionSchema& selectionSchema) {
    Fvp::PrimSelection primSelection;
    primSelection.primPath = primPath;

    auto nestedInstanceIndicesSchema = 
#if HD_API_VERSION < 66
    const_cast<HdSelectionSchema&>(selectionSchema).GetNestedInstanceIndices();
#else
    selectionSchema.GetNestedInstanceIndices();
#endif
    for (size_t iNestedInstanceIndices = 0; iNestedInstanceIndices < nestedInstanceIndicesSchema.GetNumElements(); iNestedInstanceIndices++) {
        HdInstanceIndicesSchema instanceIndicesSchema = nestedInstanceIndicesSchema.GetElement(iNestedInstanceIndices);
        auto instanceIndices = instanceIndicesSchema.GetInstanceIndices()->GetTypedValue(0);
        primSelection.nestedInstanceIndices.push_back(
            {
                instanceIndicesSchema.GetInstancer()->GetTypedValue(0),
                instanceIndicesSchema.GetPrototypeIndex()->GetTypedValue(0),
                std::vector<int>(instanceIndices.begin(), instanceIndices.end())
            }
        );
    }

    return primSelection;
}

SdfPath GetMaterialPath(const PXR_NS::HdContainerDataSourceHandle& primDataSource)
{
    if (!primDataSource) {
        return {};
    }

    HdMaterialBindingsSchema materialBindingsSchema = HdMaterialBindingsSchema::GetFromParent(primDataSource);
    if (!materialBindingsSchema.IsDefined()) {
        return {};
    }

    HdMaterialBindingSchema materialBindingSchema = materialBindingsSchema.GetMaterialBinding();
    if (!materialBindingSchema) {
        return {};
    }

    HdPathDataSourceHandle bindingPathDataSource = materialBindingSchema.GetPath();
    if (!bindingPathDataSource) {
        return {};
    }

    return bindingPathDataSource->GetTypedValue(0);
}

VtValue GetMaterialDisplacementValue(const PXR_NS::HdContainerDataSourceHandle& primDataSource, const PXR_NS::HdSceneIndexBase& sceneIndex)
{
    if (!primDataSource) {
        return {};
    }

    // Step 1: Get the material path
    const SdfPath materialPath = GetMaterialPath(primDataSource);
    if (materialPath.IsEmpty()) {
        return {};
    }

    // Step 2: Retrieve the material
    HdSceneIndexPrim materialPrim = sceneIndex.GetPrim(materialPath);
    if (!materialPrim.dataSource || (materialPrim.primType != HdPrimTypeTokens->material) ){
        return {};
    }

    HdMaterialSchema materialSchema = HdMaterialSchema::GetFromParent(materialPrim.dataSource);
    if (!materialSchema.IsDefined()) {
        return {};
    }

    // Step 3: Look for the displacement value
#if PXR_VERSION < 2403
    HdDataSourceMaterialNetworkInterface materialNetworkInterface(
        materialPath, materialSchema.GetMaterialNetwork(), primDataSource);
#else
    HdDataSourceMaterialNetworkInterface materialNetworkInterface(
        materialPath, materialSchema.GetMaterialNetwork().GetContainer(), primDataSource);
#endif

    for (auto nodeName : materialNetworkInterface.GetNodeNames()) {
        VtValue paramValue = materialNetworkInterface.GetNodeParameterValue(nodeName, HdMaterialTerminalTokens->displacement);
        if (paramValue.IsHolding<float>()) {
            return paramValue;
        }
    }

    return {};
}

PXR_NS::HdContainerDataSourceHandle AddSmoothNormals(const PXR_NS::HdContainerDataSourceHandle& meshPrimDataSource)
{
    // Check if normals are already present
    auto normalsValueDataSource = HdTypedSampledDataSource<VtArray<GfVec3f>>::Cast(HdContainerDataSource::Get(meshPrimDataSource, normalsValueLocator));
    if (normalsValueDataSource) {
        return meshPrimDataSource;
    }

    // Get required schemas/dataSources
    HdMeshSchema meshSchema = HdMeshSchema::GetFromParent(meshPrimDataSource);
    if (!meshSchema.IsDefined()) {
        return meshPrimDataSource;
    }
    HdMeshTopologySchema meshTopologySchema = meshSchema.GetTopology();
    if (!meshTopologySchema.IsDefined()) {
        return meshPrimDataSource;
    }
    auto pointsValueDataSource = HdTypedSampledDataSource<VtArray<GfVec3f>>::Cast(HdContainerDataSource::Get(meshPrimDataSource, pointsValueLocator));
    if (!pointsValueDataSource) {
        return meshPrimDataSource;
    }

    // Setup topology
    TfToken subdivScheme = PxOsdOpenSubdivTokens->none;
    if (HdTokenDataSourceHandle subdivSchemeDataSource = meshSchema.GetSubdivisionScheme()) {
        subdivScheme = subdivSchemeDataSource->GetTypedValue(0.0f);
    }
    VtIntArray holeIndices;
    if (HdIntArrayDataSourceHandle holeIndicesDataSource = meshTopologySchema.GetHoleIndices()) {
        holeIndices = holeIndicesDataSource->GetTypedValue(0.0f);
    }
    TfToken orientation = PxOsdOpenSubdivTokens->rightHanded;
    if (HdTokenDataSourceHandle orientationDataSource = meshTopologySchema.GetOrientation()) {
        orientation = orientationDataSource->GetTypedValue(0.0f);
    }
    HdMeshTopology meshTopology(
        subdivScheme, 
        orientation, 
        meshTopologySchema.GetFaceVertexCounts()->GetTypedValue(0), 
        meshTopologySchema.GetFaceVertexIndices()->GetTypedValue(0), 
        holeIndices);
    Hd_VertexAdjacency adjacency;
    adjacency.BuildAdjacencyTable(&meshTopology);

    // Compute normals
    auto points = pointsValueDataSource->GetTypedValue(0);
    auto normals = Hd_SmoothNormals::ComputeSmoothNormals(
        &adjacency, static_cast<int>(points.size()), points.cdata());

    // Add normals
    auto normalsPrimvarDataSource = HdPrimvarSchema::Builder()
        .SetInterpolation(HdPrimvarSchema::BuildInterpolationDataSource(HdPrimvarSchemaTokens->vertex))
        .SetRole(HdPrimvarSchema::BuildRoleDataSource(HdPrimvarSchemaTokens->normal))
        .SetPrimvarValue(HdRetainedTypedSampledDataSource<decltype(normals)>::New(normals))
        .Build();
    HdContainerDataSourceEditor dataSourceEditor(meshPrimDataSource);
    dataSourceEditor.Set(normalsPrimvarLocator, normalsPrimvarDataSource);
    return dataSourceEditor.Finish();
}

PXR_NS::HdContainerDataSourceHandle AddDependency(
    const PXR_NS::HdContainerDataSourceHandle& primDataSource,
    const PXR_NS::TfToken& dependencyToken,
    const PXR_NS::SdfPath& dependedOnPrimPath,
    const PXR_NS::HdDataSourceLocator& dependedOnDataSourceLocator,
    const PXR_NS::HdDataSourceLocator& affectedDataSourceLocator)
{
    HdDependencySchema::Builder builder;

    if (!dependedOnPrimPath.IsEmpty()) {
        builder.SetDependedOnPrimPath(HdRetainedTypedSampledDataSource<SdfPath>::New(dependedOnPrimPath));
    }

    builder.SetDependedOnDataSourceLocator(HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(dependedOnDataSourceLocator));

    builder.SetAffectedDataSourceLocator(HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(affectedDataSourceLocator));

    auto dependencyDataSource = HdRetainedContainerDataSource::New(dependencyToken, builder.Build());

    HdContainerDataSourceEditor dataSourceEditor(primDataSource);
    dataSourceEditor.Overlay(HdDependenciesSchema::GetDefaultLocator(), dependencyDataSource);
    return dataSourceEditor.Finish();
}

} // namespace FVP_NS_DEF
