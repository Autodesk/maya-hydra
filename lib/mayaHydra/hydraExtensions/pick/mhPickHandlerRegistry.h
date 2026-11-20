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

/// \class PickRegistration
///
/// The registration of a pick handler, shared by its clients through
/// PickRegistrationPtr. The handler is unregistered when the last client
/// releases its pointer.
///
class PickRegistration
{
public:
    MAYAHYDRALIB_API
    ~PickRegistration();

    const PXR_NS::SdfPath& GetPrefix() const { return _prefix; }

    PickRegistration(const PickRegistration&) = delete;
    PickRegistration& operator=(const PickRegistration&) = delete;

private:
    friend class PickHandlerRegistry;

    explicit PickRegistration(const PXR_NS::SdfPath& prefix)
        : _prefix(prefix)
    {
    }

    const PXR_NS::SdfPath _prefix;
};

/// \class PickHandlerRegistry
///
/// A registry of pick handlers, indexed by scene index path.
///
/// The pick handler registry has the following properties:
/// - All entries are unique.
/// - No entry is a prefix (ancestor) of another entry.
///
/// Several clients can share a prefix: each instance of the same data producer,
/// e.g. one per render override, calls Register() and keeps the registration.
/// Main thread only.
///
class PickHandlerRegistry {
public:

    MAYAHYDRALIB_API
    static PickHandlerRegistry& Instance();

    //! Register a pick handler to deal with all Hydra scene index prims
    //! under prefix.  An empty prefix, or a prefix that is the absolute
    //! root, are illegal.  If prefix is already registered with a handler of
    //! the same type, return that registration and drop pickHandler: handlers
    //! sharing a prefix must be interchangeable.
    /*!
      \return Null if prefix is illegal, if an ancestor or descendant of prefix
      is in the registry, or if prefix is registered with a handler of another
      type. The registration otherwise.
    */
    MAYAHYDRALIB_API
    PickRegistrationPtr Register(const PXR_NS::SdfPath& prefix, const PickHandlerConstPtr& pickHandler);

    //! Query the pick handler registered for that exact Hydra scene index
    //! path.  If no pick handler has been registered, return a nullptr.
    //! To obtain a handler that handles a given path, use GetHandler().
    MAYAHYDRALIB_API
    PickHandlerConstPtr RegisteredHandler(const PXR_NS::SdfPath& path) const;

    //! Get a pick handler for the argument Hydra scene index path.  This
    //! handler has a prefix that is an ancestor of the argument path.  If no
    //! pick handler is found, returns a null pointer.
    MAYAHYDRALIB_API
    PickHandlerConstPtr GetHandler(const PXR_NS::SdfPath& path) const;

    //! Set and get the pick context object for pick handlers to use. Prefer
    //! PickContextScope, which restores the previous context.
    MAYAHYDRALIB_API
    void SetPickContext(const PickContextConstPtr& context);
    MAYAHYDRALIB_API
    PickContextConstPtr GetPickContext() const;

private:

    // Called by the last owner of the registration, from ~PickRegistration().
    bool _Unregister(const PXR_NS::SdfPath& prefix);
    friend class PickRegistration;

    PickHandlerRegistry() = default;
    ~PickHandlerRegistry() = default;
    PickHandlerRegistry(const PickHandlerRegistry&) = delete;
    PickHandlerRegistry& operator=(const PickHandlerRegistry&) = delete;

    friend class PXR_NS::TfSingleton<PickHandlerRegistry>;
};

/// \class PickContextScope
///
/// Sets the pick context for the lifetime of the scope, and restores the
/// previous one on exit. Several render overrides can be live at once, so the
/// override that picks sets itself as the context for the duration of the pick.
class PickContextScope
{
public:
    explicit PickContextScope(const PickContextConstPtr& context)
        : _previous(PickHandlerRegistry::Instance().GetPickContext())
    {
        PickHandlerRegistry::Instance().SetPickContext(context);
    }

    ~PickContextScope() { PickHandlerRegistry::Instance().SetPickContext(_previous); }

    PickContextScope(const PickContextScope&) = delete;
    PickContextScope& operator=(const PickContextScope&) = delete;

private:
    const PickContextConstPtr _previous;
};

}

#endif
