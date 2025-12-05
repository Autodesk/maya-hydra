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

#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/materialConnectionSchema.h>
#include <pxr/imaging/hd/materialNodeParameterSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/purposeSchema.h>

PXR_NAMESPACE_USING_DIRECTIVE

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

bool MaterialHasDisplacement(const PXR_NS::HdSceneIndexPrim& materialPrim) {
    if (materialPrim.primType != HdPrimTypeTokens->material) {
        return false;
    }
    auto materialSchema = HdMaterialSchema::GetFromParent(materialPrim.dataSource);
    auto materialNetwork = materialSchema.GetMaterialNetwork();
    auto nodes = materialNetwork.GetNodes();
    for (const auto& nodeName : nodes.GetNames()) {
        auto displacement = nodes.Get(nodeName).GetParameters().Get(TfToken("displacement"));
        if (displacement.IsDefined()) {
            return true;
        }
    }
    auto terminals = materialNetwork.GetTerminals();
    auto displacement = terminals.Get(HdMaterialTerminalTokens->displacement);
    return displacement.IsDefined();
}

TfToken GetPurposeRenderTag(const PXR_NS::HdContainerDataSourceHandle& primDataSource)
{
    if (!primDataSource) {
        return TfToken();
    }
    HdPurposeSchema purposeSchema = HdPurposeSchema::GetFromParent(primDataSource);
    if (!purposeSchema.IsDefined()) {
        return TfToken();
    }
    HdTokenDataSourceHandle purposeDs = purposeSchema.GetPurpose();
    if (!purposeDs) {
        return TfToken();
    }
    return purposeDs->GetTypedValue(0.0);
}

} // namespace FVP_NS_DEF
