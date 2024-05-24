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
#ifndef FVP_PATH_INTERFACE_H
#define FVP_PATH_INTERFACE_H

#include "flowViewport/api.h"

#include <ufe/ufe.h>

#include <pxr/base/tf/smallVector.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/pxr.h>

UFE_NS_DEF {
class Path;
}

PXR_NAMESPACE_OPEN_SCOPE
class SdfPath;
PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

struct PrimSelectionInfo
{
    PXR_NS::SdfPath primPath;
    PXR_NS::HdDataSourceBaseHandle selectionDataSource;
};

// TODO : Use TfSmallVector for perf?
using PrimSelectionInfoVector = std::vector<PrimSelectionInfo>;

/// \class PathInterface
///
/// A pure interface class to allow for conversion between an application's
/// path, expressed as a Ufe::Path, into an SdfPath valid for a scene index.
/// To be used as a mix-in class for scene indices.
///
class PathInterface
{
public:

    //! Return the prim path corresponding to the argument application path.
    //! If no such path exists, an empty SdfPath should be returned.
    //! \return scene index path.
    FVP_API
    // TODO : Use TfSmallVector for perf?
    virtual PrimSelectionInfoVector ConvertUfeSelectionToHydra(const Ufe::Path& appPath) const = 0;

protected:

    FVP_API
    PathInterface() = default;

    FVP_API
    virtual ~PathInterface();
};

}

#endif
