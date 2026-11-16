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
//
// Environment flag for the frame publication boundary. Lives in mayaHydraLib rather
// than the mayaHydra plugin so the boundary can read it without a circular dependency.

#ifndef MAYAHYDRA_SNAPSHOT_ENV_SETTINGS_H
#define MAYAHYDRA_SNAPSHOT_ENV_SETTINGS_H

#include <mayaHydraLib/api.h>

namespace MAYAHYDRA_NS_DEF {

/// Returns true when MAYAHYDRA_SNAPSHOT or MAYAHYDRA_PIPELINE_OVERLAP is set, both
/// default off. Cached on first call. Overlap implies the boundary, because the render
/// thread names the frame it is drawing by the published frame id.
///
/// The TF_DEFINE_ENV_SETTING lives in the .cpp: this header is included from both
/// mayaHydraLib and the plugin, which would otherwise each register their own copy.
MAYAHYDRALIB_API
bool IsSnapshotEnabled();

/// Returns true when MAYAHYDRA_PIPELINE_OVERLAP is set, default off. Cached on first
/// call. When false, every overlap-gated site takes the existing synchronous path.
MAYAHYDRALIB_API
bool IsPipelineOverlapEnabled();

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_SNAPSHOT_ENV_SETTINGS_H
