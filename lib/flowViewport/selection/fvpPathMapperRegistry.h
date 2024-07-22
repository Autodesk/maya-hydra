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
#ifndef FVP_PATH_MAPPER_REGISTRY_H
#define FVP_PATH_MAPPER_REGISTRY_H

#include <flowViewport/api.h>
#include <flowViewport/selection/fvpPathMapperFwd.h>

#include <pxr/base/tf/singleton.h>
#include <pxr/usd/sdf/path.h>

#include <ufe/ufe.h>

UFE_NS_DEF {
class Path;
}

namespace FVP_NS_DEF {

/// \class PathMapperRegistry
///
/// A registry of path mappers that map from an application path to scene index
/// path, indexed by application path.
///
/// The path mapper registry has the following properties:
/// - All entries are unique.
/// - No entry is a prefix (ancestor) of another entry.
///
/// Therefore, a fallback path mapping must be implemented outside the 
/// application path to scene index path mapper.

class PathMapperRegistry {
public:

    FVP_API
    static PathMapperRegistry& Instance();

    //! Register a path mapper to deal with all application paths at or 
    //! under prefix.
    /*!
      \return False if an ancestor, descendant, or prefix itself is found in the registry, true otherwise.
    */
    FVP_API
    bool Register(const Ufe::Path& prefix, const PathMapperConstPtr& pathMapper);
    //! Unregister path mapper for prefix.
    /*!
      \return False if prefix itself was not found in the registry, true otherwise.
    */
    FVP_API
    bool Unregister(const Ufe::Path& prefix);

    //! Get a path mapper for the argument application path.  This
    //! mapper has a prefix that is an ancestor of the argument path.  If no
    //! path mapper is found, returns a null pointer.
    FVP_API
    PathMapperConstPtr GetMapper(const Ufe::Path& path) const;

private:

    PathMapperRegistry() = default;
    ~PathMapperRegistry() = default;
    PathMapperRegistry(const PathMapperRegistry&) = delete;
    PathMapperRegistry& operator=(const PathMapperRegistry&) = delete;

    friend class PXR_NS::TfSingleton<PathMapperRegistry>;
};
    
}

#endif
