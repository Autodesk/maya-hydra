//
// Copyright 2019 Luma Pictures
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

#ifndef MTOH_UTILS_H
#define MTOH_UTILS_H

#include <mayaHydraLib/mayaHydra.h>

#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/pxr.h>

#include <maya/MFrameContext.h>
#include <maya/MString.h>

#include <filesystem>
#include <string>
#include <vector>
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

} // namespace MAYAHYDRA_NS_DEF

#endif // MTOH_UTILS_H
