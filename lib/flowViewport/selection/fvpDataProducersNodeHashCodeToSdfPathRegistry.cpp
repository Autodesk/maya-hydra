//
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

#include "fvpDataProducersNodeHashCodeToSdfPathRegistry.h"
#include <pxr/base/tf/instantiateSingleton.h>

PXR_NAMESPACE_USING_DIRECTIVE
TF_INSTANTIATE_SINGLETON(FVP_NS::DataProducersNodeHashCodeToSdfPathRegistry);

// DataProducersNodeHashCodeToSdfPathRegistry does a mapping between DCC nodes hash code and Hydra
// paths. The DCC nodes registered in this class are used by data producers scene indices as a
// parent to all their Hydra primitives. The registration/unregistration in this class is automatic
// when you use the flow viewport API and provide a DCC node as a parent. This class is used when
// we select one of these DCC nodes to return the matching SdfPath so that all prims child of this
// node are highlighted.

namespace FVP_NS_DEF {

/* static */
DataProducersNodeHashCodeToSdfPathRegistry& DataProducersNodeHashCodeToSdfPathRegistry::Instance()
{
    return PXR_NS::TfSingleton<DataProducersNodeHashCodeToSdfPathRegistry>::GetInstance();
}

void DataProducersNodeHashCodeToSdfPathRegistry::Add(const unsigned long& dccNodeHashCode, const SdfPath& thePath)
{
    if (thePath.IsEmpty() || 0 == dccNodeHashCode) {
        TF_CODING_WARNING("Sending an empty SdfPath or an invalid dcc node hash code in DataProducersNodeHashCodeToSdfPathRegistry::Add, ignoring");
        return;
    }

    _SdfPathByHashCode.insert({ dccNodeHashCode, thePath });
}

void DataProducersNodeHashCodeToSdfPathRegistry::Remove(const unsigned long& dccNodeHashCode) 
{ 
    if (0 == dccNodeHashCode) {
        return;
    }

    auto it = _SdfPathByHashCode.find(dccNodeHashCode);
    if (it != _SdfPathByHashCode.end()) {
        _SdfPathByHashCode.erase(it);
    }
}

SdfPath DataProducersNodeHashCodeToSdfPathRegistry::GetPath(const unsigned long& dccNodeHashCode) const
{
    if (0 == dccNodeHashCode) {
        TF_CODING_WARNING("Sending an invalid dccNodeHashCode in DataProducersNodeHashCodeToSdfPathRegistry::GetPath");
        return {};
    }
    auto it = _SdfPathByHashCode.find(dccNodeHashCode);
    if (it != _SdfPathByHashCode.end()) {
        return (it->second);
    }
    return SdfPath();
}

} // namespace FVP_NS_DEF
