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
#ifndef FLOW_VIEWPORT_SELECTION__DATA_PRODUCERS_NODE_HASH_CODE_TO_SDFPATH_REGISTRY_H
#define FLOW_VIEWPORT_SELECTION__DATA_PRODUCERS_NODE_HASH_CODE_TO_SDFPATH_REGISTRY_H

//local headers
#include "flowViewport/api.h"

//Usd headers
#include <pxr/base/tf/singleton.h>
#include <pxr/usd/sdf/path.h>

#include <unordered_map>

namespace FVP_NS_DEF {

/// DataProducersNodeHashCodeToSdfPathRegistry does a mapping between DCC nodes hash code and Hydra paths. 
/// The DCC nodes registered in this class are used by data producers scene indices as a parent to all primitives.
/// The registration/unregistration in this class is automatic when you use the flow viewport API and provide a DCC node as a parent.
/// This class is used when we select one of these nodes to return the matching SdfPath so that all child prims of this node are highlighted.
class DataProducersNodeHashCodeToSdfPathRegistry 
{
public:
    // Access the singleton instance
    FVP_API
    static DataProducersNodeHashCodeToSdfPathRegistry& Instance();

    FVP_API
    void Add(const unsigned long& objectHandlehashCode, const PXR_NS::SdfPath& thePath);

    FVP_API
    void Remove(const unsigned long& objectHandleHashCode);

    // Returns an empty SdfPath if the objectHandle is not registered or the matching SdfPath
    // otherwise
    FVP_API
    PXR_NS::SdfPath GetPath(const unsigned long& objectHandleHashCode) const;

private:
     // Singleton, no public creation or copy.
    DataProducersNodeHashCodeToSdfPathRegistry() = default;
    ~DataProducersNodeHashCodeToSdfPathRegistry() = default;
    DataProducersNodeHashCodeToSdfPathRegistry(const DataProducersNodeHashCodeToSdfPathRegistry& ) = delete;
    DataProducersNodeHashCodeToSdfPathRegistry& operator=(const DataProducersNodeHashCodeToSdfPathRegistry& ) = delete;

    friend class PXR_NS::TfSingleton<DataProducersNodeHashCodeToSdfPathRegistry>;

    std::unordered_map<unsigned long, PXR_NS::SdfPath> _SdfPathByHashCode;

    static std::unique_ptr<DataProducersNodeHashCodeToSdfPathRegistry>   _instance;
};

} // namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_SELECTION__DATA_PRODUCERS_NODE_HASH_CODE_TO_SDFPATH_REGISTRY_H
