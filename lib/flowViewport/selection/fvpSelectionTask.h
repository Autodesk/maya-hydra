//
// Copyright 2016 Pixar
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
#ifndef FVP_SELECTION_TASK_H
#define FVP_SELECTION_TASK_H

#include "flowViewport/api.h"

#include <pxr/imaging/hd/task.h>

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE
class HdRenderIndex;
class HdSceneDelegate;
PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

/// \class SelectionTask
///
/// Placeholder for
/// https://github.com/PixarAnimationStudios/OpenUSD/blob/release/pxr/imaging/hdx/selectionTask.h
///
class SelectionTask : public PXR_NS::HdTask
{
public:
    FVP_API

    SelectionTask();

    FVP_API
    ~SelectionTask() override;

    FVP_API
    const PXR_NS::SdfPath& Id() {
        static auto id = PXR_NS::SdfPath("FlowViewportSelectionTask");
        return id;
    }

    /// Sync the render pass resources
    FVP_API
    void Sync(PXR_NS::HdSceneDelegate* delegate,
              PXR_NS::HdTaskContext* ctx,
              PXR_NS::HdDirtyBits* dirtyBits) override;
    

    /// Prepare the tasks resources
    FVP_API
    void Prepare(PXR_NS::HdTaskContext* ctx,
                 PXR_NS::HdRenderIndex* renderIndex) override;

    /// Execute render pass task
    FVP_API
    void Execute(PXR_NS::HdTaskContext* ctx) override;


private:
    int _lastVersion;

    SelectionTask(const SelectionTask &) = delete;
    SelectionTask &operator =(const SelectionTask &) = delete;
};

}

#endif //FVP_SELECTION_TASK_H

