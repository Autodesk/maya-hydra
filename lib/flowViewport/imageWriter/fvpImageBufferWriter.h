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
#ifndef FVP_IMAGE_BUFFER_WRITER_H
#define FVP_IMAGE_BUFFER_WRITER_H

#include <flowViewport/api.h>

#include <pxr/pxr.h>
#include <pxr/imaging/hio/types.h>
#include <pxr/imaging/hio/image.h>
#include <pxr/imaging/hd/tokens.h>

#include <memory>		// shared_ptr
#include <string>

PXR_NAMESPACE_OPEN_SCOPE
class HdEngine;
class TfToken;
class VtDictionary;
PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

/// \class ImageBufferWriter
///
/// Base class for writing Hydra images to files.

class ImageBufferWriter
{
public:

    using Ptr = std::shared_ptr<ImageBufferWriter>;

    FVP_API
    virtual ~ImageBufferWriter() = default;

    //! Factory method to create an image buffer writer.
    FVP_API
    static Ptr Create(
      const PXR_NS::VtDictionary& args,
      const PXR_NS::TfToken&      aov = PXR_NS::HdAovTokens->color
    );

    //! Create an image buffer writer and use it to write to the file
    //! path passed in as argument.  Returns false if the file name is
    //! empty.
    //! \return true for success.
    FVP_API
    static bool Write(
        const PXR_NS::VtDictionary& args,
        const std::string&          fileName,
        const PXR_NS::TfToken&      aov = PXR_NS::HdAovTokens->color
    );

    //! Utility method to write the content of the image buffer
    //! described by the StorageSpec to the file path passed in as argument.
    //! \return true for success.
    FVP_API
    static bool Write(
        const std::string&                   fileName,
        const PXR_NS::HioImage::StorageSpec& storageSpec
    );

    //! Utility modifier and accessor for file name to be used when calling
    //! Write() overload which takes a file argument.  Useful as a central
    //! storage location for a file name, mostly for testing purposes, as
    //! this is not thread-safe or multi-viewport ready.
    FVP_API
    static void SetFileName(const std::string& fileName);
    FVP_API
    static std::string GetFileName();

    //! Write the content of the image buffer writer to the
    //! file path passed in as argument.
    //! \return true for success.
    FVP_API
    bool Write(const std::string& fileName);

    FVP_API
    virtual unsigned int Width() const = 0;
    FVP_API
    virtual unsigned int Height() const = 0;

    FVP_API
    virtual PXR_NS::HioFormat Format() const = 0;

    FVP_API
    bool ValidSource() const;

    FVP_API
    virtual bool ValidHandle() const = 0;

    FVP_API
    virtual void* Map() = 0;

    FVP_API
    virtual void Unmap() = 0;

protected:

    FVP_API
    ImageBufferWriter() = default;

private:

    static std::string _fileName;
};

}

#endif
