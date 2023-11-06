// Copyright 2023 Autodesk
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
#ifndef FVP_MERGING_SCENE_INDEX_H
#define FVP_MERGING_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpPathInterface.h"

#include <pxr/imaging/hd/mergingSceneIndex.h>

namespace FVP_NS_DEF {

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class MergingSceneIndex;
typedef PXR_NS::TfRefPtr<MergingSceneIndex> MergingSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const MergingSceneIndex> MergingSceneIndexConstRefPtr;

/// \class MergingSceneIndex
///
/// A merging scene index that delegates conversion of application paths to
/// scene index paths to its inputs.
///
class MergingSceneIndex 
    : public PXR_NS::HdMergingSceneIndex, public PathInterface
{
public:
    FVP_API
    static MergingSceneIndexRefPtr New();

    FVP_API
    PXR_NS::SdfPath SceneIndexPath(const Ufe::Path& appPath) const override;

private:
    MergingSceneIndex();
};

}

#endif
