//
// Copyright 2019 Luma Pictures
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

#ifndef MAYAHYDRA_PLUGIN_UTILS_H
#define MAYAHYDRA_PLUGIN_UTILS_H

#include <mayaHydraLib/mayaHydra.h>

#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/renderDelegate.h> // For HdRenderSettingDescriptorList
#include <pxr/pxr.h>

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

namespace MAYAHYDRA_NS_DEF {

constexpr auto MTOH_RENDER_OVERRIDE_PREFIX = "mayaHydraRenderOverride_";

struct MtohRendererDescription
{
    MtohRendererDescription(const PXR_NS::TfToken& rn, const PXR_NS::TfToken& on, const PXR_NS::TfToken& dn)
        : rendererName(rn)
        , overrideName(on)
        , displayName(dn)
    {
    }

    PXR_NS::TfToken rendererName;
    PXR_NS::TfToken overrideName;
    PXR_NS::TfToken displayName;
};

using MtohRendererDescriptionVector = std::vector<MtohRendererDescription>;

/// Map from MtohRendererDescription::rendererName to its HdRenderSettingDescriptorList
using MtohRendererSettings
    = std::unordered_map<PXR_NS::TfToken, PXR_NS::HdRenderSettingDescriptorList, PXR_NS::TfToken::HashFunctor>;

std::string                          MtohGetRendererPluginDisplayName(const PXR_NS::TfToken& id);
const MtohRendererDescriptionVector& MtohGetRendererDescriptions();
const MtohRendererSettings&          MtohGetRendererSettings();
std::filesystem::path                MtohGetMayaHydraPluginLocation();
void                                 MtohSetMayaHydraPluginLocation(const std::filesystem::path& mayaHydraLocation);

// Register a Hydra renderer to Maya's renderer registry.
bool registerRenderer(const MtohRendererDescription& desc);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_PLUGIN_UTILS_H
