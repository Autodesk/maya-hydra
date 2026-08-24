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

#include "mhRenderingColorSpaceResolvingSceneIndex.h"


#include <ufe/colorManagementHandler.h>

#include <ufeExtensions/Global.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

MhRenderingColorSpaceResolvingSceneIndexRefPtr
MhRenderingColorSpaceResolvingSceneIndex::New(const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
    return TfCreateRefPtr(new MhRenderingColorSpaceResolvingSceneIndex(inputSceneIndex));
}

MhRenderingColorSpaceResolvingSceneIndex::MhRenderingColorSpaceResolvingSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
    : Fvp::RenderingColorSpaceResolvingSceneIndex(inputSceneIndex)
{
}

/// Resolve rendering color space according to the rules defined in the base class.
///
bool MhRenderingColorSpaceResolvingSceneIndex::UseAuthoredRenderingColorSpace(
    const TfToken&     authoredRenderingColorSpace,
    const std::string& applicationRenderingColorSpace) const
{
    // If rendering color space is authored and valid with the OCIO config file, use it.
    if (IsKnownColorSpace(authoredRenderingColorSpace.GetString())) {
        // Authored rendering color space doesn't match the one from Maya prefs.
        // Let the user know that we're using the one from the active render settings node.
        if (authoredRenderingColorSpace.GetString() != applicationRenderingColorSpace) {
            TF_WARN(
                "Authored rendering color space '%s' in the active render settings prim "
                "doesn't match the value from Maya preferences '%s'. "
                "Using the value from the active render settings prim.",
                authoredRenderingColorSpace.GetText(),
                applicationRenderingColorSpace.c_str());
            return true;
        }
        return true;
    }
    // If rendering color space is authored but not valid with the OCIO config file, let the
    // user know that we're using the one from Maya prefs.
    TF_WARN(
        "Authored rendering color space '%s' in the active render settings prim is not "
        "known to the OCIO config file. "
        "Using the value from Maya preferences '%s'.",
        authoredRenderingColorSpace.GetText(),
        applicationRenderingColorSpace.c_str());
    return false;
}

/// If color management is not enabled in Maya, this will return the default rendering color space
/// from the OCIO config file.
std::string MhRenderingColorSpaceResolvingSceneIndex::GetApplicationRenderingColorSpace() const
{
    const auto colorManagement
        = Ufe::ColorManagementHandler::colorManagementHandler(UfeExtensions::getMayaRunTimeId());
    if (!TF_VERIFY(colorManagement, "No color management handler registered for Maya runtime"))
        return {};

    return colorManagement->getRenderingSpaceName();
}

/// Check if the given color space is known to the OCIO config file.
bool MhRenderingColorSpaceResolvingSceneIndex::IsKnownColorSpace(
    const std::string& colorSpace) const
{
    const auto colorManagement
        = Ufe::ColorManagementHandler::colorManagementHandler(UfeExtensions::getMayaRunTimeId());
    if (!TF_VERIFY(colorManagement, "No color management handler registered for Maya runtime"))
        return false;
    return colorManagement->isKnownColorSpace(colorSpace);
}

} // namespace MAYAHYDRA_NS_DEF
