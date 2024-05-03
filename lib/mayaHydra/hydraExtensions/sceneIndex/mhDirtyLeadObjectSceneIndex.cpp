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

//Local headers
#include "mhDirtyLeadObjectSceneIndex.h"

// Hydra headers
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/primvarsSchema.h>

PXR_NAMESPACE_USING_DIRECTIVE

// The MhDirtyLeadObjectSceneIndex class is responsible for dirtying the current and previous maya selection lead objects prim
// path when a change in the lead object selection has happened.
namespace MAYAHYDRA_NS_DEF {

namespace {
    //Handle primsvars:overrideWireframeColor in Storm for wireframe selection highlighting color
    TF_DEFINE_PRIVATE_TOKENS(
         _primVarsTokens,
 
         (overrideWireframeColor)    // Works in HdStorm to override the wireframe color
     );

    const HdDataSourceLocatorSet primvarsColorsLocatorSet{  HdPrimvarsSchema::GetDefaultLocator().Append(_primVarsTokens->overrideWireframeColor),
                                                            HdPrimvarsSchema::GetDefaultLocator().Append(HdTokens->displayColor)
                                                         };
}

void MhDirtyLeadObjectSceneIndex::dirtyLeadObjectRelatedPrims(const PXR_NS::SdfPath& previousLeadObjectPath, const PXR_NS::SdfPath& currentLeadObjectPath)
{
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedPrimEntries;
    if (!currentLeadObjectPath.IsEmpty()) {
        dirtiedPrimEntries.push_back({currentLeadObjectPath, primvarsColorsLocatorSet});
    }
    if (!previousLeadObjectPath.IsEmpty()) {
        dirtiedPrimEntries.push_back({ previousLeadObjectPath, primvarsColorsLocatorSet });
    }
    if (dirtiedPrimEntries.size() > 0){
        _SendPrimsDirtied(dirtiedPrimEntries);
    }
}

}//end of namespace MAYAHYDRA_NS_DEF
