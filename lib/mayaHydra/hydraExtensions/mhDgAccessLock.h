//
// Copyright 2026 Autodesk, Inc. All rights reserved.
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

#ifndef MAYAHYDRALIB_MH_DG_ACCESS_LOCK_H
#define MAYAHYDRALIB_MH_DG_ACCESS_LOCK_H

#include <mayaHydraLib/api.h>

#include <mutex>

namespace MAYAHYDRA_NS_DEF {

using DgAccessMutexType = std::recursive_mutex;

/// The single mutex serializing Maya Dependency Graph reads performed on behalf of Hydra.
///
/// Hydra pulls prim data from multiple threads while Maya may be evaluating the DG in
/// parallel, and the Maya API tolerates neither concurrent reads of the same node nor reads
/// racing against evaluation. There must be exactly one such mutex for the whole library:
/// per-translation-unit mutexes would let two call sites believe they are mutually exclusive
/// when they are not.
///
/// The mutex is recursive because locked methods legitimately call one another, for example
/// MayaHydraMeshAdapter::GetMeshTopology() calls GetDisplayStyle(), and
/// MayaHydraShapeAdapter::GetBoundingBox() calls GetTransform().
MAYAHYDRALIB_API
DgAccessMutexType& GetDgAccessMutex();

class DgAccessLock
{
public:
    DgAccessLock() : _lock(GetDgAccessMutex()) { }

    DgAccessLock(const DgAccessLock&) = delete;
    DgAccessLock& operator=(const DgAccessLock&) = delete;

private:
    std::lock_guard<DgAccessMutexType> _lock;
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRALIB_MH_DG_ACCESS_LOCK_H
