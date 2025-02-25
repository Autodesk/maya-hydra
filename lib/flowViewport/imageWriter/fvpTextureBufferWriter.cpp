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

#include <flowViewport/imageWriter/fvpTextureBufferWriter.h>

#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hdx/types.h>
#include <pxr/base/vt/value.h>
#include <pxr/base/vt/dictionary.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

template<typename T>
T* getPtr(const VtDictionary& args, const char* key)
{
    auto found = args.find(key);
    if (found == args.end() || 
	!found->second.IsHolding<T*>()) {
        return nullptr;
    }

    return found->second.Get<T*>();
}

HgiTextureHandle getTextureHandle(
    const VtDictionary& args,
    const TfToken&      aovToken
)
{
    auto engine = getPtr<HdEngine>(args, "engine");
    if (!engine) {
        return {};
    }

    VtValue aov;
    return (engine->GetTaskContextData(aovToken, &aov) &&
	    aov.IsHolding<HgiTextureHandle>()) ?
      aov.Get<HgiTextureHandle>() : HgiTextureHandle();
}

}

namespace FVP_NS_DEF {

TextureBufferWriter::TextureBufferWriter(
    const VtDictionary& args,
    const TfToken&      aov
) : ImageBufferWriter(), 
    _textureHandle(getTextureHandle(args, aov)),
    _hgi(getPtr<Hgi>(args, "hgi"))
{}

unsigned int TextureBufferWriter::Dim(unsigned int i) const
{
    return _textureHandle ? 
        _textureHandle->GetDescriptor().dimensions[i] : 0;
}

unsigned int TextureBufferWriter::Width() const
{
    return Dim(0);
}

unsigned int TextureBufferWriter::Height() const
{
    return Dim(1);
}

HioFormat TextureBufferWriter::Format() const
{
    return _textureHandle ? HdxGetHioFormat(
        _textureHandle->GetDescriptor().format
    ) : HioFormatInvalid;
}

bool TextureBufferWriter::ValidHandle() const
{
    return bool(_textureHandle);
}

void* TextureBufferWriter::Map()
{
    if (!_textureHandle || !_hgi) {
        return nullptr;
    }

    size_t size = 0;
    _mappedTextureBuffer = HdStTextureUtils::HgiTextureReadback(
        _hgi, _textureHandle, &size);
    return _mappedTextureBuffer.get();
}

// No-op.  Could consider releasing the _mappedTextureBuffer,
// but currently this is done at TextureBufferWriter destruction.
void TextureBufferWriter::Unmap()
{}

}
