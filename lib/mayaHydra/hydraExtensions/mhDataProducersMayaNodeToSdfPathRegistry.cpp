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

#include "mhDataProducersMayaNodeToSdfPathRegistry.h"
#include <pxr/base/tf/instantiateSingleton.h>

PXR_NAMESPACE_USING_DIRECTIVE
TF_INSTANTIATE_SINGLETON(MAYAHYDRA_NS_DEF::MhDataProducersMayaNodeToSdfPathRegistry);

namespace MAYAHYDRA_NS_DEF {

/* static */
MhDataProducersMayaNodeToSdfPathRegistry& MhDataProducersMayaNodeToSdfPathRegistry::Instance()
{
    return PXR_NS::TfSingleton<MhDataProducersMayaNodeToSdfPathRegistry>::GetInstance();
}

void MhDataProducersMayaNodeToSdfPathRegistry::Add(const unsigned long& objectHandleHashCode, const SdfPath& thePath)
{
    if (thePath.IsEmpty() || 0 == objectHandleHashCode) {
        TF_CODING_WARNING("Sending an empty SdfPath or an invalid objectHandle has code in MhDataProducersMayaNodeToSdfPathRegistry::Add, ignoring");
        return;
    }

    _SdfPathByHashCode.insert({ objectHandleHashCode, thePath });
}

void MhDataProducersMayaNodeToSdfPathRegistry::Remove(const unsigned long& objectHandleHashCode) 
{ 
    if (0 == objectHandleHashCode) {
        return;
    }

    auto it = _SdfPathByHashCode.find(objectHandleHashCode);
    if (it != _SdfPathByHashCode.end()) {
        _SdfPathByHashCode.erase(it);
    }
}

SdfPath MhDataProducersMayaNodeToSdfPathRegistry::GetPath(const unsigned long& objectHandleHashCode) const
{
    if (0 == objectHandleHashCode) {
        TF_CODING_WARNING("Sending an invalid objectHandleHashCode in MhDataProducersMayaNodeToSdfPathRegistry::GetPath");
        return {};
    }
    auto it = _SdfPathByHashCode.find(objectHandleHashCode);
    if (it != _SdfPathByHashCode.end()) {
        return (it->second);
    }
    return SdfPath();
}

} // namespace MAYAUSD_NS_DEF
