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
#ifndef FVP_SCENE_INDEX_UTILS_H
#define FVP_SCENE_INDEX_UTILS_H

#include <flowViewport/api.h>

#include <pxr/imaging/hd/filteringSceneIndex.h>

namespace FVP_NS_DEF {

/// \class InputSceneIndexUtils
///
/// Utility class to factor out common single input scene index functionality.
///
template<class T>
class InputSceneIndexUtils
{
public:

    InputSceneIndexUtils(
        const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex
    ) 
#ifdef CODE_COVERAGE_WORKAROUND
        : _inputSceneIndex(inputSceneIndex)
#endif
    {}

    // At time of writing directly accessing _GetInputSceneIndex() from
    // clang coverage build causes crash.  PPT, 24-Jan-2024.
    const PXR_NS::HdSceneIndexBaseRefPtr& GetInputSceneIndex() const
    {
#ifdef CODE_COVERAGE_WORKAROUND
        return _inputSceneIndex;
#else
        return static_cast<const T*>(this)->_GetInputSceneIndex();
#endif
    }

private:

#ifdef CODE_COVERAGE_WORKAROUND
    const PXR_NS::HdSceneIndexBaseRefPtr _inputSceneIndex;
#endif
};

}

#endif
