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

#include <pxr/pxr.h>
#include <pxr/base/tf/smallVector.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/usd/sdf/path.h>

UFE_NS_DEF {
class Path;
}

namespace FVP_NS_DEF {

struct PrimSelectionInfo
{
    PXR_NS::SdfPath primPath;
    PXR_NS::HdDataSourceBaseHandle selectionDataSource;
};

// Using TfSmallVector to optimize for selections that map to a few prims,
// which is likely going to be the bulk of use cases.
using PrimSelectionInfoVector = PXR_NS::TfSmallVector<PrimSelectionInfo, 8>;

/// \class PathInterface
///
/// A pure interface class to allow for conversion between an application's
/// path, expressed as a Ufe::Path, into SdfPaths valid for a scene index
/// and selection data sources.
/// To be used as a mix-in class for scene indices.
///
class PathInterface
{
public:

    //! Return the prim path corresponding to the argument application path.
    //! If no such path exists, an empty SdfPath should be returned.
    //! \return scene index path.
    FVP_API
    virtual PrimSelectionInfoVector ConvertUfePathToHydraSelections(const Ufe::Path& appPath) const = 0;

protected:

    FVP_API
    PathInterface() = default;

    FVP_API
    virtual ~PathInterface();
};

}

#endif
