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
#ifndef FVP_SELECTION_TYPES_H
#define FVP_SELECTION_TYPES_H

#include "flowViewport/flowViewport.h"

#include <pxr/pxr.h>
#include <pxr/base/tf/smallVector.h>
#include <pxr/usd/sdf/path.h>

namespace FVP_NS_DEF {

// Based on USD's HdInstanceIndicesSchema : 
// https://github.com/PixarAnimationStudios/OpenUSD/blob/59992d2178afcebd89273759f2bddfe730e59aa8/pxr/imaging/hd/instanceIndicesSchema.h
struct InstancesSelection {
    PXR_NS::SdfPath instancerPath;
    int prototypeIndex;
    std::vector<int> instanceIndices;

    inline bool operator==(const InstancesSelection &rhs) const {
        return instancerPath == rhs.instancerPath
            && prototypeIndex == rhs.prototypeIndex
            && instanceIndices == rhs.instanceIndices;
    }
};

// Based on USD's HdSelectionSchema : 
// https://github.com/PixarAnimationStudios/OpenUSD/blob/59992d2178afcebd89273759f2bddfe730e59aa8/pxr/imaging/hd/selectionSchema.h
struct PrimSelection
{
    PXR_NS::SdfPath primPath;
    std::vector<InstancesSelection> nestedInstanceIndices;

    inline bool operator==(const PrimSelection &rhs) const {
        return primPath == rhs.primPath
            && nestedInstanceIndices == rhs.nestedInstanceIndices;
    }
};

// Using TfSmallVector to optimize for selections that map to a few prims,
// which is likely going to be the bulk of use cases.
using PrimSelections = PXR_NS::TfSmallVector<PrimSelection, 8>;

}

#endif
