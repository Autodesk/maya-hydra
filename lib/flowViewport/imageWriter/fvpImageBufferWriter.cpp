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

#include <flowViewport/imageWriter/fvpImageBufferWriter.h>

#include <pxr/base/tf/scoped.h>
#include <pxr/imaging/hdx/types.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

std::string ImageBufferWriter::_fileName{};

bool ImageBufferWriter::ValidSource() const
{
    return ValidHandle() && 
        (Width() != 0) && (Height() != 0) &&
        (Format() != HioFormatInvalid);
}

bool ImageBufferWriter::Write(const std::string& fileName)
{
    if (!ValidSource()) {
        return false;
    }

    HioImage::StorageSpec storage;
    storage.width = Width();
    storage.height = Height();
    storage.format = Format();
    storage.flipped = true;
    storage.data = Map();

    if (!storage.data) {
        return false;
    }

    TfScoped unmapGuard([this](){ Unmap(); });

    const auto image = HioImage::OpenForWriting(fileName);
    return image && image->Write(storage);
}

/* static */
bool ImageBufferWriter::Write(
    const PXR_NS::VtDictionary& args,
    const std::string&          fileName,
    bool                        useHVT,
    const PXR_NS::TfToken&      aov
)
{
    auto writer = Create(args, useHVT, aov);
    return (writer ? writer->Write(fileName) : false);
}

/* static */
bool ImageBufferWriter::Write(
    const std::string&           fileName, 
    const HioImage::StorageSpec& storageSpec
)
{
    if (!storageSpec.data) {
        return false;
    }

    const auto image = HioImage::OpenForWriting(fileName);
    return image && image->Write(storageSpec);
}

/* static */
void ImageBufferWriter::SetFileName(const std::string& fileName)
{
    _fileName = fileName;
}

/* static */
std::string ImageBufferWriter::GetFileName()
{
    return _fileName;
}

}
