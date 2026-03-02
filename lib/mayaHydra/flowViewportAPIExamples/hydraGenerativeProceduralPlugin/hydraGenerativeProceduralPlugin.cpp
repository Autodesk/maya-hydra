//
// Copyright 2025 Autodesk, Inc. All rights reserved.
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

#include "hydraGenerativeProceduralPlugin.h"
#include "hydraGenerativeProcedural.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((sceneIndexPluginName, "HydraGenerativeProceduralPlugin"))
);

////////////////////////////////////////////////////////////////////////////////
// Plugin registrations
////////////////////////////////////////////////////////////////////////////////

TF_REGISTRY_FUNCTION(TfType)
{
    HdGpGenerativeProceduralPluginRegistry::Define<
        HydraGenerativeProceduralPlugin,
        HdGpGenerativeProceduralPlugin>();
}

////////////////////////////////////////////////////////////////////////////////
// Implementations
////////////////////////////////////////////////////////////////////////////////

HydraGenerativeProceduralPlugin::
HydraGenerativeProceduralPlugin() = default;

HydraGenerativeProceduralPlugin::~HydraGenerativeProceduralPlugin() = default;

HdGpGenerativeProcedural *
HydraGenerativeProceduralPlugin::Construct(
    const SdfPath &proceduralPrimPath)
{
    return new HydraGenerativeProcedural(proceduralPrimPath);
}

PXR_NAMESPACE_CLOSE_SCOPE
