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

#include <flowViewport/api.h>

#ifdef CODE_COVERAGE_WORKAROUND
#include <pxr/imaging/hd/sceneIndex.h>
#endif

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/retainedDataSource.h>
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

} // namespace FVP_NS_DEF

#endif // FVP_UTILS_H
