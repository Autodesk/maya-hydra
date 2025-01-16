//
// Copyright 2024 Autodesk
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
#ifndef FVP_PATH_MAPPER_H
#define FVP_PATH_MAPPER_H

#include <flowViewport/api.h>
#include <flowViewport/selection/fvpPathMapperFwd.h>
#include <flowViewport/selection/fvpSelectionTypes.h>

#include <pxr/usd/sdf/path.h>

#include <ufe/ufe.h>

UFE_NS_DEF {
class Path;
}

namespace FVP_NS_DEF {

/// \class PathMapper
///
/// The path handler performs application path to scene index path mapping.
///
/// This is useful for selection highlighting, where an application selection
/// path is converted to a path to a Hydra scene index prim that must be
/// highlighted.

class PathMapper
{
public:

    //! Return the prim path(s) corresponding to the argument application path,
    //! as well as their associated selection data source(s).
    //! If no such selected path exists, an empty container should be returned.
    //! \return Selected prim paths and their associated selection data sources.
    FVP_API
    virtual PrimSelections UfePathToPrimSelections(const Ufe::Path& appPath) const = 0;

protected:

    FVP_API
    PathMapper() = default;
};

class PrimPathsCountOutOfRangeException : public std::out_of_range
{
public:
    PrimPathsCountOutOfRangeException(size_t min, size_t max, size_t actual);
};

}

#endif
