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
#ifndef FVP_TEXTURE_BUFFER_WRITER_H
#define FVP_TEXTURE_BUFFER_WRITER_H

#include <flowViewport/api.h>

#include <flowViewport/imageWriter/fvpImageBufferWriter.h>

#include <pxr/imaging/hgi/texture.h>
#include <pxr/imaging/hdSt/textureUtils.h>

namespace FVP_NS_DEF {

/// \class TextureBufferWriter
///
/// Concrete class for writing GPU-created texture Hydra images to
/// files.

class TextureBufferWriter : public ImageBufferWriter
{
public:

    FVP_API
    TextureBufferWriter(
      const PXR_NS::VtDictionary& args,
      const PXR_NS::TfToken&      aov
    );

    unsigned int Width() const override;
    unsigned int Height() const override;

    PXR_NS::HioFormat Format() const override;

    bool ValidHandle() const override;

private:

    unsigned int Dim(unsigned int i) const;

    void* Map() override;

    void Unmap() override;

    PXR_NS::HgiTextureHandle _textureHandle;
    PXR_NS::HdStTextureUtils::AlignedBuffer<uint8_t> _mappedTextureBuffer;
    PXR_NS::Hgi* _hgi;          // Unowned pointer.
};

}

#endif
