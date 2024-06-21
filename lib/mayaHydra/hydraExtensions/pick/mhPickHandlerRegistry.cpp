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

#include <mayaHydraLib/mayaHydra.h>
#include <mayaHydraLib/pick/mhPickHandlerRegistry.h>
#include <mayaHydraLib/pick/mhPickHandler.h>
#include <mayaHydraLib/pick/mhPickContext.h>

#include <pxr/base/tf/instantiateSingleton.h>

#include <map>

using namespace MayaHydra;

namespace {

std::map<PXR_NS::SdfPath, PickHandlerConstPtr> pickHandlers;
PickContextConstPtr pickContext = nullptr;

}

PXR_NAMESPACE_OPEN_SCOPE
TF_INSTANTIATE_SINGLETON(PickHandlerRegistry);
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

/* static */
PickHandlerRegistry& PickHandlerRegistry::Instance()
{
    return PXR_NS::TfSingleton<PickHandlerRegistry>::GetInstance();
}

bool PickHandlerRegistry::Register(const SdfPath& prefix, const PickHandlerConstPtr& pickHandler)
{
    // Can't register an empty path, or an absolute root path prefix.
    if (prefix.IsEmpty() || prefix.IsAbsoluteRootPath()) {
        return false;
    }

    // No entries yet?  Add.
    if (pickHandlers.empty()) {
        pickHandlers.emplace(prefix, pickHandler);
        return true;
    }

    // At least one entry.  Skip all entries before argument prefix.  The
    // iterator points to an entry with matching or greater key.
    auto it = pickHandlers.lower_bound(prefix);

    // Reached the end with no entries before argument prefix?  Last entry is
    // strictly smaller than.  If the last entry is a prefix to the one we're
    // trying to add, fail, else add.
    if (it == pickHandlers.end()) {
        auto rit = pickHandlers.rbegin();
        if (prefix.HasPrefix(rit->first)) {
            return false;
        }
        pickHandlers.emplace_hint(it, prefix, pickHandler);
        return true;
    }

    // Already in the map or a descendant already in the map?  Fail.
    if (it->first.HasPrefix(prefix)) {
        return false;
    }

    // At the first entry and it's not a match or a descendant?  Add entry.
    if (it == pickHandlers.begin()) {
        pickHandlers.emplace_hint(it, prefix, pickHandler);
        return true;
    }

    // Somewhere in the middle of the map.  Go back one entry.  Is it a match,
    // a descendant, or an ancestor?  Fail.
    it = std::prev(it);
    if (it->first.HasPrefix(prefix) || prefix.HasPrefix(it->first)) {
        return false;
    }

    // All checks pass: add entry.
    pickHandlers.emplace_hint(it, prefix, pickHandler);
    return true;
}

bool PickHandlerRegistry::Unregister(const SdfPath& prefix)
{
    auto found = pickHandlers.find(prefix);
    if (found == pickHandlers.end()) {
        return false;
    }
    pickHandlers.erase(found);
    return true;
}

PickHandlerConstPtr PickHandlerRegistry::GetHandler(const SdfPath& path) const
{
    // No entries yet?  Fail.
    if (pickHandlers.empty()) {
        return {};
    }

    // At least one entry.  Skip all entries before argument prefix.  The
    // iterator points to an entry with matching or greater key.
    auto it = pickHandlers.lower_bound(path);

    // Reached the end with no entries before argument path?  Last entry is
    // strictly smaller than, so if it's a prefix to the path we're querying,
    // return handler, else fail.
    if (it == pickHandlers.end()) {
        auto rit = pickHandlers.rbegin();
        return (path.HasPrefix(rit->first)) ? rit->second : nullptr;
    }

    // Not at the end.  Query is exactly in the map?  Return handler.
    if (it->first == path) {
        return it->second;
    }

    // Query path is a prefix to what's in the map?  Fail.
    if (it->first.HasPrefix(path)) {
        return nullptr;
    }

    // At the first entry.  If query is a descendant, return handler, else fail.
    if (it == pickHandlers.begin()) {
        return (path.HasPrefix(it->first)) ? it->second : nullptr;
    }

    // Somewhere in the middle of the map.  Go back one entry.  If query is a
    // descendant, return handler, else fail.
    it = std::prev(it);
    return (path.HasPrefix(it->first)) ? it->second : nullptr;
}

void PickHandlerRegistry::SetPickContext(const PickContextConstPtr& context)
{
    pickContext = context;
}

PickContextConstPtr PickHandlerRegistry::GetPickContext() const
{
    return pickContext;
}

}
