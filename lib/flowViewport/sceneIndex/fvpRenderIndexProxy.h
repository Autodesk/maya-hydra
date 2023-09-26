//
// Copyright 2022 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
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
#ifndef FVP_RENDER_INDEX_PROXY_H
#define FVP_RENDER_INDEX_PROXY_H

#include "flowViewport/api.h"

#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/mergingSceneIndex.h>

PXR_NAMESPACE_OPEN_SCOPE
class HdRenderIndex;
PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

/// \class RenderIndexProxy
///
/// Class to protect access to the render index, and provide a merging
/// scene index under Flow viewport control.
///
/// The merging scene index accessed through the Hydra render has hard-coded
/// downstream filtering scene indices.  This render index proxy provides its
/// own merging scene index, after which we can easily insert downstream
/// filtering scene indices.
///
/// FLOW_VIEWPORT_TODO  At time of writing, the renderIndex data member is
/// unused.  Re-evaluate the responsibilities, future extension, and naming of
/// this class.  PPT, 22-Sep-2023.

class RenderIndexProxy
{
public:

    FVP_API
    RenderIndexProxy(PXR_NS::HdRenderIndex* renderIndex);

    FVP_API
    void InsertSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr& inputScene,
        const PXR_NS::SdfPath&                scenePathPrefix,
        bool                                  needsPrefixing = true);

    FVP_API
    void RemoveSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr &inputScene);

    // Return the additional Flow Viewport merging scene index onto which input
    // scenes are added.  Returned as a base scene index to preserve
    // encapsulation.
    FVP_API
    PXR_NS::HdSceneIndexBaseRefPtr GetMergingSceneIndex() const;

private:

    PXR_NS::HdRenderIndex* const      _renderIndex{nullptr};
    PXR_NS::HdMergingSceneIndexRefPtr _mergingSceneIndex;
};

}

#endif
