//
// Copyright 2026 Autodesk, Inc. All rights reserved.
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
#ifndef MAYAHYDRA_RENDER_ITEM_OVERRIDE_CREATOR_H
#define MAYAHYDRA_RENDER_ITEM_OVERRIDE_CREATOR_H

#include <mayaHydraLib/api.h>

// -----------------------------------------------------------------------------
// The MPxRenderItemOverride that turns the geometry Maya carries for a shape
// (a render item override carrier) into the MayaHydraRenderItemPayload
// payload that MayaHydraRenderItemOverrideAdapter serves to Hydra.
//
// This is the mechanism only. Whether to use it is the plugin's decision: the
// plugin registers create() for every draw classification through
// MHWRender::MRenderItemOverrideRegistry - Maya's PRIVATE render-item override
// API (maya/MPxRenderItemOverridePrivate.h) - and tells the scene index
// (MayaHydraSceneIndex::SetRenderItemOverrideEnabled). Maya has no gate of
// its own: a registered creator IS the opt-in, and Maya only claims shapes
// while the viewport is in render override mode.
//
// Declared only when Maya provides that header
// (MAYA_HAS_RENDER_ITEM_OVERRIDE_PRIVATE); without it there is nothing to
// register and the stock render-item path runs.
// -----------------------------------------------------------------------------
#if defined(MAYA_HAS_RENDER_ITEM_OVERRIDE_PRIVATE)

#include <maya/MObject.h>
#include <maya/MPxRenderItemOverridePrivate.h>
#include <maya/MString.h>

#include <vector>

namespace MayaHydraRenderItemOverrideCreator {

//! The creator to register (MRenderItemOverrideRegistry::Creator): one
//! override instance per carried object.
MAYAHYDRALIB_API MHWRender::MPxRenderItemOverride* create(const MObject& node);

//! Every draw classification create() serves.
MAYAHYDRALIB_API const std::vector<MString>& drawClassifications();

//! The motion-blur samples to register with (MAYAHYDRA_MOTION_SAMPLING_MODE).
MAYAHYDRALIB_API MHWRender::MPxRenderItemOverride::ShutterSamples shutterSamples();

} // namespace MayaHydraRenderItemOverrideCreator

#endif // MAYA_HAS_RENDER_ITEM_OVERRIDE_PRIVATE

#endif // MAYAHYDRA_RENDER_ITEM_OVERRIDE_CREATOR_H
