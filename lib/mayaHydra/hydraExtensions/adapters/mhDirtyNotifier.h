// Copyright 2026 Autodesk
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
// MayaHydra::DirtyNotifier is a thin adapter-aware wrapper around Fvp::DirtyNotifier.
// It accepts a MayaHydraAdapter* and extracts the scene index and prim path automatically,
// eliminating the two-argument boilerplate at every adapter call site.
//
#ifndef MAYAHYDRA_MH_DIRTY_NOTIFIER_H
#define MAYAHYDRA_MH_DIRTY_NOTIFIER_H

#include <mayaHydraLib/api.h>
#include <mayaHydraLib/mayaHydra.h>

#include <flowViewport/fvpDirtyNotifier.h>

#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE
class MayaHydraAdapter;
class HdRetainedSceneIndex;
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

/// \class DirtyNotifier
///
/// Convenience wrapper around Fvp::DirtyNotifier for adapter call sites.
///
/// Constructs from a MayaHydraAdapter pointer, extracting the owning scene
/// index and prim path automatically, so adapter call sites can write:
///
///   MayaHydra::DirtyNotifier notifier(adapter);   // from a pointer
///   DirtyNotifier notifier(this);                  // inside an adapter method
///
/// instead of the verbose two-argument base-class form:
///
///   Fvp::DirtyNotifier notifier(*adapter->GetMayaHydraSceneIndex(), adapter->GetID());
///
/// All Fvp::DirtyNotifier dirty*() methods and flush() are inherited unchanged.
///
/// dirtyUVs() and dirtyTangents() are Maya-specific and live here (rather than
/// in Fvp::DirtyNotifier) so that flowViewport does not embed Maya primvar name
/// strings. They use MayaHydraAdapterTokens directly.
class DirtyNotifier : public Fvp::DirtyNotifier
{
public:
    MAYAHYDRALIB_API
    explicit DirtyNotifier(PXR_NS::MayaHydraAdapter* adapter);

    /// Same as Fvp::DirtyNotifier(sceneIndex, primPath); exposes MayaHydra dirty*()
    /// helpers (e.g. dirtyUVs) without an adapter pointer (unit tests, harnesses).
    MAYAHYDRALIB_API
    DirtyNotifier(PXR_NS::HdRetainedSceneIndex& sceneIndex, const PXR_NS::SdfPath& primPath);

    /// Invalidates the UV primvar (primvars/st).
    MAYAHYDRALIB_API DirtyNotifier& dirtyUVs();
    /// Invalidates the tangents primvar (primvars/tangents).
    MAYAHYDRALIB_API DirtyNotifier& dirtyTangents();
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_MH_DIRTY_NOTIFIER_H
