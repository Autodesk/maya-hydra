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

#include "mhGenerativeProceduralResolvingSceneIndex.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

MAYAHYDRALIB_API
void MhGenerativeProceduralResolvingSceneIndex::AddExcludedSceneRoot(const SdfPath& sceneRoot) {
    _excludedSceneRoots.emplace(sceneRoot);
}

MAYAHYDRALIB_API
MhGenerativeProceduralResolvingSceneIndexRefPtr
MhGenerativeProceduralResolvingSceneIndex::New(const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
#if PXR_VERSION < 2511
    // Windows fails to link mayaHydraLib against shared usd_hdGp here: the import
    // library exposes only exported DLL symbols. HdGpGenerativeProceduralResolvingSceneIndex::New()
    // needs ctor symbols that are not exported before USD 25.11;
    return TfCreateRefPtr(new MhGenerativeProceduralResolvingSceneIndex(inputSceneIndex));
#else
    return TfCreateRefPtr(new MhGenerativeProceduralResolvingSceneIndex(
        HdGpGenerativeProceduralResolvingSceneIndex::New(inputSceneIndex)));
#endif

}

} // namespace MAYAHYDRA_NS_DEF