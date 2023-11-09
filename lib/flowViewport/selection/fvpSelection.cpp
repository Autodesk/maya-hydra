//
// Copyright 2022 Pixar
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

#include "flowViewport/selection/fvpSelection.h"

#include "pxr/imaging/hd/selectionSchema.h"
#include "pxr/imaging/hd/selectionsSchema.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

using namespace Fvp;

class SelectionSchemaFullySelectedBuilder {
public:
    SelectionSchemaFullySelectedBuilder() : _builder() {
        _builder.SetFullySelected(
            HdRetainedTypedSampledDataSource<bool>::New(true));
    }

    HdContainerDataSourceHandle Build() { return _builder.Build(); }

private:
    HdSelectionSchema::Builder _builder;
};

SelectionSchemaFullySelectedBuilder selectionBuilder;

}

namespace FVP_NS_DEF {

HdDataSourceBaseHandle 
Selection::_PrimSelectionState::GetVectorDataSource() const
{
    return HdSelectionsSchema::BuildRetained(
        selectionSources.size(), selectionSources.data()
    );
};

bool
Selection::Add(const PXR_NS::SdfPath& primPath)
{
    if (primPath.IsEmpty()) {
        return false;
    }

    _pathToState[primPath].selectionSources.push_back(selectionBuilder.Build());

    return true;
}

bool Selection::Remove(const PXR_NS::SdfPath& primPath)
{
    return (!primPath.IsEmpty() && (_pathToState.erase(primPath) == 1));
}

void
Selection::Clear()
{
    _pathToState.clear();
}

void Selection::Replace(const PXR_NS::SdfPathVector& selection)
{
    Clear();

    for (const auto& primPath : selection) {
        if (primPath.IsEmpty()) {
            continue;
        }
        _pathToState[primPath].selectionSources.push_back(
            selectionBuilder.Build());
    }
}

void Selection::RemoveHierarchy(const PXR_NS::SdfPath& primPath)
{
    auto it = _pathToState.lower_bound(primPath);
    while (it != _pathToState.end() && it->first.HasPrefix(primPath)) {
        it = _pathToState.erase(it);
    }
}

bool Selection::IsEmpty() const
{
    return _pathToState.empty();
}

bool Selection::IsFullySelected(const SdfPath& primPath) const
{
    return _pathToState.find(primPath) != _pathToState.end();
}

bool Selection::HasFullySelectedAncestorInclusive(const SdfPath& primPath) const
{
    // FLOW_VIEWPORT_TODO  Prefix tree would be much higher performance
    // than iterating over the whole selection, especially for a large
    // selection.  PPT, 13-Sep-2023.
    for(const auto& entry : _pathToState) {
        if (primPath.HasPrefix(entry.first)) {
            return true;
        }
    }
    return false;
}

SdfPathVector Selection::GetFullySelectedPaths() const
{
    SdfPathVector fullySelectedPaths;
    fullySelectedPaths.reserve(_pathToState.size());
    for(const auto& entry : _pathToState) {
        fullySelectedPaths.emplace_back(entry.first);
    }
    return fullySelectedPaths;
}

HdDataSourceBaseHandle Selection::GetVectorDataSource(
    const PXR_NS::SdfPath& primPath
) const
{
    auto it = _pathToState.find(primPath);
    return (it != _pathToState.end()) ? 
        it->second.GetVectorDataSource() : nullptr;
}

}
