//
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

#ifndef MAYAHYDRA_OPTIONAL_SCENE_INDEX_PLUGINS_H
#define MAYAHYDRA_OPTIONAL_SCENE_INDEX_PLUGINS_H

#include <mayaHydraLib/mayaHydra.h>

#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>

namespace MAYAHYDRA_NS_DEF {

/// Registers the optional scene index plugins that renderers offer for the reprs
/// \p authoredReprs, so that the renderers that cannot draw them natively get them.
///
/// A renderer describes such a plugin in the plugInfo.json type entry of the scene
/// index plugin, in renderer and Hydra terms only:
///   "optionalForRenderer": the renderer display name to register it for,
///   "implementsReprs":     the repr tokens it implements,
///   "insertionPhase":      the scene index plugin registry phase to register it at.
/// A plugin is registered, once per process, when it implements at least one of
/// \p authoredReprs. Reading the metadata does not load the renderer. The registration
/// passes the input argument "enabled" = true: an optional plugin is inert without it,
/// because USD's default (hybrid) scene index ordering puts every plugin that has a JSON
/// entry ("loadWithRenderer") in the chain, whether a host registered it or not.
///
/// Setting the environment variable MAYAHYDRA_DISABLE_OPTIONAL_SCENE_INDEX_PLUGINS to 1
/// skips the registration.
///
/// Must be called before any render index is created, since the scene index plugins are
/// instantiated when a render index is built.
void RegisterOptionalSceneIndexPlugins(const PXR_NS::TfTokenVector& authoredReprs);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_OPTIONAL_SCENE_INDEX_PLUGINS_H
