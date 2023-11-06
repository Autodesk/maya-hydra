// Copyright 2023 Autodesk
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
#ifndef FVP_SELECTION_INTERFACE_H
#define FVP_SELECTION_INTERFACE_H

#include "flowViewport/api.h"

#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE
class SdfPath;
PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

/// \class SelectionInterface
///
/// A pure interface class to allow querying a scene index for selected status 
/// without inspecting data sources in scene prims.  To be used as a mix-in
/// class for scene indices.
///
class SelectionInterface
{
public:

    //! Returns whether the prim path is a fully selected prim.
    //! If no such path exists, false is returned.
    //! \return Whether the argument path is a fully selected prim.
    FVP_API
    virtual bool IsFullySelected(const PXR_NS::SdfPath& primPath) const = 0;

    //! Returns whether the prim path is a fully selected prim, or has an
    //! ancestor that is a fully selected prim.  If no such path exists, false
    //! is returned.
    //! \return Whether the argument path is a fully selected prim, or has a
    //! fully selected ancestor.
    FVP_API
    virtual bool HasFullySelectedAncestorInclusive(const PXR_NS::SdfPath& primPath) const = 0;

protected:

    FVP_API
    SelectionInterface() = default;

    FVP_API
    virtual ~SelectionInterface();
};

}

#endif
