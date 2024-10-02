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

#include "flowViewport/selection/fvpSelectionTypes.h"

#include <ufe/ufe.h>

#include <pxr/pxr.h>
#include <pxr/base/tf/smallVector.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/usd/sdf/path.h>

#include <stdexcept>

UFE_NS_DEF {
class Path;
}

namespace FVP_NS_DEF {

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

    //! Return the prim path(s) corresponding to the argument application path,
    //! as well as their associated selection data source(s).
    //! If no such selected path exists, an empty container should be returned.
    //! \return Selected prim paths and their associated selection data sources.
    FVP_API
    virtual PrimSelections UfePathToPrimSelections(const Ufe::Path& appPath) const = 0;

    //! Return the prim path corresponding to the argument application path,
    //! for when an application path maps to at most a single prim path.
    //! If no such path exists, an empty SdfPath should be returned.
    //! \return Scene index path.
    FVP_API
    PXR_NS::SdfPath SceneIndexPath(const Ufe::Path& appPath) const;

    //! Return the prim paths corresponding to the argument application path.
    //! If no such paths exist, an empty SdfPathVector should be returned.
    //! \return Scene index paths.
    FVP_API
    PXR_NS::SdfPathVector SceneIndexPaths(const Ufe::Path& appPath) const;

protected:

    FVP_API
    PathInterface() = default;

    FVP_API
    virtual ~PathInterface();
};

class PrimPathsCountOutOfRangeException : public std::out_of_range
{
public:
    PrimPathsCountOutOfRangeException(size_t min, size_t max, size_t actual);
};

}

#endif
