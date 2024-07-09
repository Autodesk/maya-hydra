//
// Copyright 2018 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//

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

#ifndef FLOW_VIEWPORT_SHADERS_DISCOVERY_PLUGIN_H
#define FLOW_VIEWPORT_SHADERS_DISCOVERY_PLUGIN_H

#include <pxr/usd/ndr/discoveryPlugin.h>

PXR_NAMESPACE_OPEN_SCOPE

class FlowViewportShadersDiscoveryPlugin;
TF_DECLARE_WEAK_AND_REF_PTRS(FlowViewportShadersDiscoveryPlugin);

class FlowViewportShadersDiscoveryPlugin : public NdrDiscoveryPlugin
{
  public:
    FlowViewportShadersDiscoveryPlugin() = default;
    ~FlowViewportShadersDiscoveryPlugin() override = default;
    NdrNodeDiscoveryResultVec DiscoverNodes(const Context& context) override;
    const NdrStringVec& GetSearchURIs() const override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // FLOW_VIEWPORT_SHADERS_DISCOVERY_PLUGIN_H
