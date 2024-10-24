//
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
#ifndef FVP_SELECTION_H
#define FVP_SELECTION_H

#include "flowViewport/api.h"
#include "flowViewport/selection/fvpSelectionFwd.h"
#include "flowViewport/selection/fvpSelectionTypes.h"

#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/usd/sdf/path.h>

#include <map>

namespace FVP_NS_DEF {

/// \class Selection
///
/// Represents selection in the Flow Viewport.
///
/// Hydra's HdSelection class has support for component selections (edges and
/// points), but the following limitations:
/// - No support for remove, replace, or clear operations.
/// - No support for querying selection state of ancestors.
///
/// It would be desirable to add these capabilities to HdSelection and
/// move support to OpenUSD.
///
class Selection
{
public:

    using PrimSelectionsMap = std::map<PXR_NS::SdfPath, PrimSelections>;

    // Create a reference-counted Selection.
    FVP_API
    static SelectionPtr New();

    // Add primPath to selection and return true if the argument is not empty.
    FVP_API
    bool Add(const PrimSelection& primSelection);

    // Returns true if the removal was successful, false otherwise.
    FVP_API
    bool Remove(const PrimSelection& primSelection);

    // Replace the selection with the contents of the argument vector.
    // Any empty primPath in the argument will be skipped.
    FVP_API
    void Replace(const PrimSelections& primSelections);

    FVP_API
    void Replace(const Selection& selection);

    // Remove all entries from the selection.
    FVP_API
    void Clear();

    // Remove the argument and all descendants from the selection.
    FVP_API
    void RemoveHierarchy(const PXR_NS::SdfPath& primPath);

    FVP_API
    bool IsEmpty() const;

    FVP_API
    bool IsFullySelected(const PXR_NS::SdfPath& primPath) const;

    // Returns true if the argument is itself selected, or any of its ancestors
    // is selected, up to the specified topmost ancestor. By default, the topmost
    // ancestor is set to the absolute root path, so that all ancestors are considered.
    FVP_API
    bool HasFullySelectedAncestorInclusive(const PXR_NS::SdfPath& primPath, const PXR_NS::SdfPath& topmostAncestor = PXR_NS::SdfPath::AbsoluteRootPath()) const;

    // Returns true if the argument itself is selected, or a descendant of the
    // argument.
    FVP_API
    bool HasDescendantInclusive(const PXR_NS::SdfPath& primPath) const;

    // Returns true if the argument itself is selected, or an ancestor or
    // descendant of the argument is selected.
    FVP_API
    bool HasAncestorOrDescendantInclusive(const PXR_NS::SdfPath& primPath) const;

    // Returns the paths to all fully selected ancestors of the prim up to the specified
    // topmost ancestor. If the prim is itself selected, its path will also be returned.
    // By default, the topmost ancestor is set to the absolute root path, so that all 
    // ancestors are considered.
    FVP_API
    PXR_NS::SdfPathVector FindFullySelectedAncestorsInclusive(const PXR_NS::SdfPath& primPath, const PXR_NS::SdfPath& topmostAncestor = PXR_NS::SdfPath::AbsoluteRootPath()) const;

    FVP_API
    PXR_NS::SdfPathVector GetFullySelectedPaths() const;

    // Return the vector data source of the argument prim if selected, else
    // a null pointer.
    FVP_API
    PXR_NS::HdDataSourceBaseHandle
    GetVectorDataSource(const PXR_NS::SdfPath& primPath) const;

    FVP_API
    PrimSelections GetPrimSelections(const PXR_NS::SdfPath& primPath) const;

    PrimSelectionsMap::const_iterator begin() const;
    PrimSelectionsMap::const_iterator end() const;

private:
    
    // Maps prim path to selections to be returned by the vector data
    // source at locator selections.
    PrimSelectionsMap _pathToSelections;
};

}

#endif
