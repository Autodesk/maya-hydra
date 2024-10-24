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
#include "sceneIndex/fvpPathInterface.h"

#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/selectionSchema.h>

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

} // namespace FVP_NS_DEF
