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

#include "fvpUtils.h"

namespace FVP_NS_DEF {

#ifdef CODE_COVERAGE_WORKAROUND
void leakSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& si) {
    // Must place the leaked scene index vector on the heap, as a by-value
    // vector will have its destructor called at process exit, which calls
    // the vector element destructors and triggers the crash. 
    static std::vector<PXR_NS::HdSceneIndexBaseRefPtr>* leakedSi{nullptr};
    if (!leakedSi) {
        leakedSi = new std::vector<PXR_NS::HdSceneIndexBaseRefPtr>;
    }
    leakedSi->push_back(si);
}
#endif

} // namespace FVP_NS_DEF
