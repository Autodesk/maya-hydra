//
// Copyright 2025 Autodesk
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

#include <flowViewport/imageWriter/fvpRenderBufferWriter.h>

#ifdef VIEWPORT_TOOLBOX
#include <hvt/engine/framePass.h>
#endif

#include <pxr/imaging/hdSt/hioConversions.h>
#include <pxr/imaging/hdx/taskController.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/base/vt/dictionary.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

HdRenderBuffer* getRenderBuffer(
    const VtDictionary&    args, bool useHVT,
    const PXR_NS::TfToken& aovToken
)
{
#ifdef VIEWPORT_TOOLBOX
    auto framePass = Fvp::ImageBufferWriter::GetPtr<hvt::FramePass>(args, "framePass");
    if (framePass) {
        return framePass ? framePass->GetRenderBuffer(aovToken) : nullptr;
    }
#else
    auto taskController = Fvp::ImageBufferWriter::GetPtr<HdxTaskController>(args, "taskController");
    if (taskController) {
        return taskController ? taskController->GetRenderOutput(aovToken) : nullptr;
    }
#endif
    return nullptr;
}

}

namespace FVP_NS_DEF {

RenderBufferWriter::RenderBufferWriter(
    const PXR_NS::VtDictionary& args,
    bool                        useHVT,
    const TfToken&              aov)
    : ImageBufferWriter()
    , _renderBuffer(getRenderBuffer(args, useHVT, aov))
{}

unsigned int RenderBufferWriter::Width() const
{
    return _renderBuffer ? _renderBuffer->GetWidth() : 0;
}

unsigned int RenderBufferWriter::Height() const
{
    return _renderBuffer ? _renderBuffer->GetHeight() : 0;
}

HioFormat RenderBufferWriter::Format() const
{
    return _renderBuffer ? HdStHioConversions::GetHioFormat(
        _renderBuffer->GetFormat()) : HioFormatInvalid;
}

bool RenderBufferWriter::ValidHandle() const
{
    return bool(_renderBuffer);
}

void* RenderBufferWriter::Map()
{
    return _renderBuffer ? _renderBuffer->Map() : nullptr;
}

void RenderBufferWriter::Unmap()
{
    if (_renderBuffer) {
        _renderBuffer->Unmap();
    }
}

}
