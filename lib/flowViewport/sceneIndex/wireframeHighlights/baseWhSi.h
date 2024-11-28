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
#ifndef FVP_BASE_WIREFRAME_HIGHLIGHT_SCENE_INDEX_H
#define FVP_BASE_WIREFRAME_HIGHLIGHT_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/selection/fvpSelectionFwd.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/fvpWireframeColorInterface.h"

#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/usd/sdf/path.h>

#include <functional>
#include <set>
#include <unordered_map>

namespace FVP_NS_DEF {

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class BaseWhSi;
typedef PXR_NS::TfRefPtr<BaseWhSi> BaseWhSiRefPtr;
typedef PXR_NS::TfRefPtr<const BaseWhSi> BaseWhSiConstRefPtr;

/// \class BaseWhSi
///
/// Uses Hydra HdRepr to add wireframe representation to selected objects
/// and their descendants.
///
class BaseWhSi 
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<BaseWhSi>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;
};

PXR_NS::HdContainerDataSourceHandle MakeWireframe(const PXR_NS::HdContainerDataSourceHandle& dataSource, const PXR_NS::GfVec4f& color);

}

#endif // FVP_BASE_WIREFRAME_HIGHLIGHT_SCENE_INDEX_H
