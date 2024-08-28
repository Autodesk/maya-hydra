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

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

std::unique_ptr<MhDataProducersMayaNodeToSdfPathRegistry>   MhDataProducersMayaNodeToSdfPathRegistry::_instance;

MhDataProducersMayaNodeToSdfPathRegistry& MhDataProducersMayaNodeToSdfPathRegistry::Get()
{
    if (! _instance){
        _instance.reset(new MhDataProducersMayaNodeToSdfPathRegistry());
    }
    return *_instance;
}

void MhDataProducersMayaNodeToSdfPathRegistry::Add(const MObjectHandle& objectHandle, const SdfPath& thePath)
{
    if (thePath.IsEmpty() || ! objectHandle.isValid() ) {
        TF_CODING_WARNING("Sending an empty SdfPath or an invalid objectHandle in MhDataProducersMayaNodeToSdfPathRegistry::Add, ignoring");
        return;
    }

    _SdfPathByObjectHandle.insert({ objectHandle, thePath });
}

void MhDataProducersMayaNodeToSdfPathRegistry::Remove(unsigned long& objectHandleHashCode) 
{ 
    if (0 == objectHandleHashCode) {
        return;
    }

    for (auto it = _SdfPathByObjectHandle.begin(); it != _SdfPathByObjectHandle.end(); ++it) {
        if (it->first.hashCode() == objectHandleHashCode) {
            _SdfPathByObjectHandle.erase(it);
            return;
        }
    }
}

void MhDataProducersMayaNodeToSdfPathRegistry::Remove(const MObjectHandle& objectHandle)
{
    if(! objectHandle.isValid()){
        return;
    }
    auto it = _SdfPathByObjectHandle.find(objectHandle);
    if (it != _SdfPathByObjectHandle.end()) { 
        _SdfPathByObjectHandle.erase(it);
    }
}

SdfPath MhDataProducersMayaNodeToSdfPathRegistry::GetPath(const MObjectHandle& objectHandle) const
{
    if (!objectHandle.isValid()) {
        TF_CODING_WARNING("Sending an invalid objectHandle in MhDataProducersMayaNodeToSdfPathRegistry::GetPath");
        return {};
    }
    auto it = _SdfPathByObjectHandle.find(objectHandle);
    if (it != _SdfPathByObjectHandle.end()) {
        return (it->second);
    }
    return SdfPath();
}

} // namespace MAYAUSD_NS_DEF
