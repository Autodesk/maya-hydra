//
// Copyright 2024 Autodesk
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

#include <flowViewport/selection/fvpPrefixPathMapper.h>

// Need Pixar namespace for TF_ diagnostics macros.
PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

PrefixPathMapper::PrefixPathMapper(
    Ufe::Rtid              rtid,
    const Ufe::Path&       appPathPrefix, 
    const PXR_NS::SdfPath& sceneIndexPathPrefix
) : _rtid(rtid), _appPathPrefix(appPathPrefix), 
    _sceneIndexPathPrefix(sceneIndexPathPrefix)
{}

PXR_NS::SdfPath PrefixPathMapper::SceneIndexPath(const Ufe::Path& appPath) const
{
    // We only handle scene items from our assigned run time ID.
    if (appPath.runTimeId() != _rtid) {
        return {};
    }

    // If the data model object application path does not match the path we
    // translate, return an empty path.
    if (!appPath.startsWith(_appPathPrefix)) {
        return {};
    }

    // The scene index path is composed of 2 parts, in order:
    // 1) The scene index path prefix, which is fixed on construction.
    // 2) The second segment of the UFE path, with each UFE path component
    //    becoming an SdfPath component.
    PXR_NS::SdfPath sceneIndexPath = _sceneIndexPathPrefix;
    TF_AXIOM(appPath.nbSegments() == 2);
    const auto& secondSegment = appPath.getSegments()[1];
    for (const auto& pathComponent : secondSegment) {
        sceneIndexPath = sceneIndexPath.AppendChild(
            TfToken(pathComponent.string()));
    }
    return sceneIndexPath;
}

}
