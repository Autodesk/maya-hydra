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
#ifndef MAYA_HYDRA_DATA_PRODUCERS_MAYA_NODE_TO_SDFPATH_REGISTRY_H
#define MAYA_HYDRA_DATA_PRODUCERS_MAYA_NODE_TO_SDFPATH_REGISTRY_H

//MayaHydra headers
#include "mayaHydraLib/api.h"

//Usd headers
#include <pxr/base/tf/singleton.h>
#include <pxr/usd/sdf/path.h>

//Maya headers
#include <maya/MObjectHandle.h>

#include <unordered_map>

namespace MAYAHYDRA_NS_DEF {

/// MhDataProducersMayaNodeToSdfPathRegistry does a mapping between Maya nodes and Hydra paths. 
/// The maya nodes registered in this class are used by data producers as a parent to all primitives.
/// The registration/unregistration in this class is automatic when you use the flow viewport API and provide a maya node as a parent.
/// This class is used when we select one of these maya nodes to return the matching SdfPath so that all prims child of this maya node are highlighted.
class MhDataProducersMayaNodeToSdfPathRegistry 
{
public:
    // Access the singleton instance
    MAYAHYDRALIB_API
    static MhDataProducersMayaNodeToSdfPathRegistry& Instance();

    MAYAHYDRALIB_API
    void Add(const unsigned long& objectHandlehashCode, const PXR_NS::SdfPath& thePath);

    MAYAHYDRALIB_API
    void Remove(const unsigned long& objectHandleHashCode);

    // Returns an empty SdfPath if the objectHandle is not registered or the matching SdfPath
    // otherwise
    MAYAHYDRALIB_API
    PXR_NS::SdfPath GetPath(const unsigned long& objectHandleHashCode) const;

private:
     // Singleton, no public creation or copy.
    MhDataProducersMayaNodeToSdfPathRegistry() = default;
    ~MhDataProducersMayaNodeToSdfPathRegistry() = default;
    MhDataProducersMayaNodeToSdfPathRegistry(const MhDataProducersMayaNodeToSdfPathRegistry& ) = delete;
    MhDataProducersMayaNodeToSdfPathRegistry& operator=(const MhDataProducersMayaNodeToSdfPathRegistry& ) = delete;

    friend class PXR_NS::TfSingleton<MhDataProducersMayaNodeToSdfPathRegistry>;

    std::unordered_map<unsigned long, PXR_NS::SdfPath> _SdfPathByHashCode;

    static std::unique_ptr<MhDataProducersMayaNodeToSdfPathRegistry>   _instance;
};

} // namespace MAYAHYDRA_NS_DEF

#endif //MAYA_HYDRA_DATA_PRODUCERS_MAYA_NODE_TO_SDFPATH_REGISTRY_H
