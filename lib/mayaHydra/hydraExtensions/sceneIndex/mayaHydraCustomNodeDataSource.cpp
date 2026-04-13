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
//
// Implements MayaHydraCustomNodeDataSource: the Hydra data source that
// exposes a custom Maya plugin node's type name and non-default attribute
// values. Delegates attribute reading to MayaHydraCustomDagAdapter.
//

#include "mayaHydraCustomNodeDataSource.h"

#include <mayaHydraLib/adapters/customDagAdapter.h>
#include <mayaHydraLib/adapters/tokens.h>

#include <pxr/base/vt/dictionary.h>
#include <pxr/imaging/hd/retainedDataSource.h>

PXR_NAMESPACE_OPEN_SCOPE

MayaHydraCustomNodeDataSource::MayaHydraCustomNodeDataSource(
    MayaHydraCustomDagAdapter* adapter)
    : _adapter(adapter)
{
}

TfTokenVector MayaHydraCustomNodeDataSource::GetNames()
{
    return {
        MayaHydraAdapterTokens->mayaTypeName,
        MayaHydraAdapterTokens->mayaAttributes
    };
}

HdDataSourceBaseHandle MayaHydraCustomNodeDataSource::Get(const TfToken& name)
{
    if (name == MayaHydraAdapterTokens->mayaTypeName) {
        return HdRetainedTypedSampledDataSource<TfToken>::New(
            _adapter->GetMayaTypeName());
    }
    if (name == MayaHydraAdapterTokens->mayaAttributes) {
        return HdRetainedTypedSampledDataSource<VtDictionary>::New(
            _adapter->GetNonDefaultMayaAttributes());
    }
    return nullptr;
}

PXR_NAMESPACE_CLOSE_SCOPE
