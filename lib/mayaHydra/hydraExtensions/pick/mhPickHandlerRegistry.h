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
#ifndef MH_PICK_HANDLER_REGISTRY_H
#define MH_PICK_HANDLER_REGISTRY_H

#include <mayaHydraLib/api.h>
#include <mayaHydraLib/pick/mhPickHandlerFwd.h>
#include <mayaHydraLib/pick/mhPickContextFwd.h>

#include <pxr/base/tf/singleton.h>
#include <pxr/usd/sdf/path.h>

namespace MAYAHYDRA_NS_DEF {

/// \class PickHandlerRegistry
///
/// A registry of pick handlers, indexed by scene index path.
///
/// The pick handler registry has the following properties:
/// - All entries are unique.
/// - No entry is a prefix (ancestor) of another entry.
///
class PickHandlerRegistry {
public:

    MAYAHYDRALIB_API
    static PickHandlerRegistry& Instance();

    //! Register a pick handler to deal with all Hydra scene index prims
    //! under prefix.  An empty prefix, or a prefix that is the absolute
    //! root, are illegal.
    /*!
      \return False if an ancestor, descendant, or prefix itself is found in the registry, true otherwise.
    */
    MAYAHYDRALIB_API
    bool Register(const PXR_NS::SdfPath& prefix, const PickHandlerConstPtr& pickHandler);
    //! Unregister pick handler for prefix.
    /*!
      \return False if prefix itself was not found in the registry, true otherwise.
    */
    MAYAHYDRALIB_API
    bool Unregister(const PXR_NS::SdfPath& prefix);

    //! Get a pick handler for the argument Hydra scene index path.  This
    //! handler has a prefix that is an ancestor of the argument path.  If no
    //! pick handler is found, returns a null pointer.
    MAYAHYDRALIB_API
    PickHandlerConstPtr GetHandler(const PXR_NS::SdfPath& path) const;

    //! Set and get the pick context object for pick handlers to use.
    MAYAHYDRALIB_API
    void SetPickContext(const PickContextConstPtr& context);
    MAYAHYDRALIB_API
    PickContextConstPtr GetPickContext() const;

private:

    PickHandlerRegistry() = default;
    ~PickHandlerRegistry() = default;
    PickHandlerRegistry(const PickHandlerRegistry&) = delete;
    PickHandlerRegistry& operator=(const PickHandlerRegistry&) = delete;

    friend class PXR_NS::TfSingleton<PickHandlerRegistry>;
};
    
}

#endif
