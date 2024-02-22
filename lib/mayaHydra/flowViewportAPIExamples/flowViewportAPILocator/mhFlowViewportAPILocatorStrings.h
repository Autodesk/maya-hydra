//
// Copyright 2024 Autodesk, Inc. All rights reserved.
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

#ifndef _MAYAHYDRA_FLOW_VIEWPORT_API_MAYA_LOCATOR_STRINGS_
#define _MAYAHYDRA_FLOW_VIEWPORT_API_MAYA_LOCATOR_STRINGS_

////////////////////////////////////////////////////////////////////////////////
//
// MhFlowViewportAPILocator Node hydra plugin localization strings
//
////////////////////////////////////////////////////////////////////////////////

#include <maya/MStringResource.h>
#include <maya/MStringResourceId.h>
// MStringResourceIds contain plug-in id, unique resource id for
// each string and the default value for the string.

#define kMhFlowViewportAPILocatorNodePluginId				"MhFlowViewportAPILocator"

// If a MStringResourceId is added to this list, please register in registerMStringRes() 
// in MhFlowViewportAPILocator.cpp

#define rMayaHydraNotLoadedStringError  MStringResourceId( kMhFlowViewportAPILocatorNodePluginId, "rMayaHydraNotLoadedStringError", "You need to load the mayaHydra plugin before creating this node.")

#endif //_MAYAHYDRA_FLOW_VIEWPORT_API_MAYA_LOCATOR_STRINGS_