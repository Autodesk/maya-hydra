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
#ifndef FVP_PREFIX_PATH_MAPPER_H
#define FVP_PREFIX_PATH_MAPPER_H

#include <flowViewport/api.h>
#include <flowViewport/selection/fvpPathMapper.h>

#include <ufe/rtid.h>
#include <ufe/path.h>

namespace FVP_NS_DEF {

/// \class PrefixPathMapper
///
/// This simple path handler performs application path to scene index path
/// mapping by substituting a scene index prefix for an application path prefix.
/// The prefix mapper applies only to application paths of a specific UFE run
/// time.
///

class PrefixPathMapper : public PathMapper
{
public:

    FVP_API
    PrefixPathMapper(
        Ufe::Rtid              rtid,
        const Ufe::Path&       appPathPrefix, 
        const PXR_NS::SdfPath& sceneIndexPathPrefix
    );

    FVP_API
    PrimSelections UfePathToPrimSelections(const Ufe::Path& appPath) const override;

private:

    const Ufe::Rtid       _rtid{0}; // 0 is invalid
    const Ufe::Path       _appPathPrefix;
    const PXR_NS::SdfPath _sceneIndexPathPrefix;
};

}

#endif
