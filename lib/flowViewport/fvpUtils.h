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
#ifndef FVP_UTILS_H
#define FVP_UTILS_H

#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <flowViewport/api.h>
#include <flowViewport/selection/fvpSelectionTypes.h>

#ifdef CODE_COVERAGE_WORKAROUND
#include <pxr/imaging/hd/sceneIndex.h>
#endif

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>

namespace FVP_NS_DEF {

// At time of writing, the last reference removal causing destruction of a 
// scene index crashes on Windows with clang code coverage compilation here:
//
// usd_tf!std::_Atomic_storage<int,4>::compare_exchange_strong+0x38 [inlined in usd_tf!pxrInternal_v0_23__pxrReserved__::Tf_RefPtr_UniqueChangedCounter::_RemoveRef+0x62]
//
// To work around this, leak the scene index to avoid its destruction.
// PPT, 24-Jan-2024.

#ifdef CODE_COVERAGE_WORKAROUND
void FVP_API leakSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& si);
#endif

/// A convenience data source implementing the primvar schema from
/// a triple of primvar value, interpolation and role. The latter two
/// are given as tokens. The value can be given either as data source
/// or as thunk returning a data source which is evaluated on each
/// Get.
class PrimvarDataSource final : public PXR_NS::HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(PrimvarDataSource);

    PXR_NS::TfTokenVector GetNames() override {
        return {PXR_NS::HdPrimvarSchemaTokens->primvarValue,
                PXR_NS::HdPrimvarSchemaTokens->interpolation,
                PXR_NS::HdPrimvarSchemaTokens->role};
    }

    PXR_NS::HdDataSourceBaseHandle Get(const PXR_NS::TfToken &name) override {
        if (name == PXR_NS::HdPrimvarSchemaTokens->primvarValue) {
            return _primvarValueSrc;
        }
        if (name == PXR_NS::HdPrimvarSchemaTokens->interpolation) {
            return
                PXR_NS::HdPrimvarSchema::BuildInterpolationDataSource(
                    _interpolation);
        }
        if (name == PXR_NS::HdPrimvarSchemaTokens->role) {
            return
                PXR_NS::HdPrimvarSchema::BuildRoleDataSource(
                    _role);
        }

        return nullptr;
    }

private:
    PrimvarDataSource(
        const PXR_NS::HdDataSourceBaseHandle &primvarValueSrc,
        const PXR_NS::TfToken &interpolation,
        const PXR_NS::TfToken &role)
        : _primvarValueSrc(primvarValueSrc)
        , _interpolation(interpolation)
        , _role(role)
    {
    }

    PXR_NS::HdDataSourceBaseHandle _primvarValueSrc;
    PXR_NS::TfToken _interpolation;
    PXR_NS::TfToken _role;
};

PXR_NS::HdDataSourceBaseHandle createSelectionDataSource(const PrimSelection& selection);

template<typename PathContainer>
auto FindSelfOrFirstParent(const PXR_NS::SdfPath& path, const PathContainer& container) -> decltype(container.cend()) {
    PXR_NS::SdfPath currPath = path;
    while (!currPath.IsEmpty()) {
        auto foundIt = container.find(currPath);
        if (foundIt != container.cend()) {
            return foundIt;
        }
        else {
            currPath = currPath.GetParentPath();
        }
    }
    return container.cend();
}

inline
auto FindSelfOrFirstChild(const PXR_NS::SdfPath& path, const std::set<PXR_NS::SdfPath>& pathSet) -> decltype(pathSet.cend()) {
    auto itPath = pathSet.lower_bound(path);
    if (itPath != pathSet.cend() && itPath->HasPrefix(path)) {
        return itPath;
    }
    return pathSet.cend();
}

template<typename MapValueType>
auto FindSelfOrFirstChild(const PXR_NS::SdfPath& path, const std::map<PXR_NS::SdfPath, MapValueType>& pathMap) -> decltype(pathMap.cend()) {
    auto itPath = pathMap.lower_bound(path);
    if (itPath != pathMap.cend() && itPath->first.HasPrefix(path)) {
        return itPath;
    }
    return pathMap.cend();
}

// Return a Fvp::PrimSelection equivalent to the given Hydra selection
Fvp::PrimSelection ConvertHydraToFvpSelection(const PXR_NS::SdfPath& primPath, const PXR_NS::HdSelectionSchema& selectionSchema);

// Get the path to the prim's bound material.
PXR_NS::SdfPath GetMaterialPath(const PXR_NS::HdContainerDataSourceHandle& primDataSource);

// Return the displacement value from the given prim data source's assigned material
PXR_NS::VtValue GetMaterialDisplacementValue(const PXR_NS::HdContainerDataSourceHandle& primDataSource, const PXR_NS::HdSceneIndexBase& sceneIndex);

// Computes and adds the normals primvar with smooth normals. If normals are already present, does nothing.
PXR_NS::HdContainerDataSourceHandle AddSmoothNormals(const PXR_NS::HdContainerDataSourceHandle& meshPrimDataSource);

// Add an entry to the __dependencies data source
PXR_NS::HdContainerDataSourceHandle AddDependency(
    const PXR_NS::HdContainerDataSourceHandle& primDataSource,
    const PXR_NS::TfToken& dependencyToken,
    const PXR_NS::SdfPath& dependedOnPrimPath,
    const PXR_NS::HdDataSourceLocator& dependedOnDataSourceLocator,
    const PXR_NS::HdDataSourceLocator& affectedDataSourceLocator);

} // namespace FVP_NS_DEF

#endif // FVP_UTILS_H
