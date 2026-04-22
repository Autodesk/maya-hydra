//
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
#ifndef MAYAHYDRALIB_MESH_ADAPTER_TEST_UTILS_H
#define MAYAHYDRALIB_MESH_ADAPTER_TEST_UTILS_H

#include <mayaHydraLib/api.h>

#include <string>
#include <unordered_set>

namespace MAYAHYDRA_NS_DEF {

/// Exposed for unit tests to verify kMeshParamAttributeNames stays in sync with NodeDirtiedCallback.
MAYAHYDRALIB_API
const std::unordered_set<std::string>& GetMeshParamAttributeNamesForTest();

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRALIB_MESH_ADAPTER_TEST_UTILS_H
