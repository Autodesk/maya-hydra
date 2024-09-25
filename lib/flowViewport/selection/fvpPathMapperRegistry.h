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
#include <flowViewport/sceneIndex/fvpPathInterface.h>

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
/// A fallback path mapper can be provided to implement a path mapping chain of
/// responsibility, for an application's native data model paths.  This is
/// useful as the path mapper uses plugin prim path prefixes to convert between
/// a data model path to one (or more) scene index prim path(s).  The
/// application data model has no plugin data model Hydra scene index prim path
/// prefix, so the application data model should be made the fallback, if no
/// other path mapper prefix matches.

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

    //! Set a fallback path mapper.  If set, it will be returned by
    //! GetMapper() if no mapper is registered for a given argument path.
    //! A null pointer argument removes the fallback path mapper.
    FVP_API
    void SetFallbackMapper(const PathMapperConstPtr& pathMapper);
    FVP_API
    PathMapperConstPtr GetFallbackMapper() const;

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
    
/**
 * @brief Get the prim selections for a given application path.
 *
 * If an application path corresponds to a scene index prim, this function will
 * return one or more prim selections for it.  If no such scene index prim
 * exists, the returned prim selections will be empty.  It retrieves the
 * appropriate path mapper from the path mapper registry and invokes it on the
 * argument appPath.
 *
 * @param[in] appPath The application path for which prim selections should be returned.
 * @return Zero or more prim selections.
 */
FVP_API
PrimSelections ufePathToPrimSelections(const Ufe::Path& appPath);

}

#endif
