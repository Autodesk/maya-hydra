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

} // namespace FVP_NS_DEF

#endif // FVP_UTILS_H
