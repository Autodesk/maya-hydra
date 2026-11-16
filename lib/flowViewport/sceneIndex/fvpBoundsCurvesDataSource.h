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
#ifndef FVP_BOUNDS_CURVES_DATA_SOURCE_H
#define FVP_BOUNDS_CURVES_DATA_SOURCE_H

#include "flowViewport/api.h"

#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/dataSourceLocator.h>

namespace FVP_NS_DEF {

/// Returns the data source of a basisCurves prim drawing the edges of the box given by the extent
/// of \p primSource, in \p wireframeColor, in the secondary graphics pass and without culling.
///
/// The xform, visibility, instancedBy, primOrigin and extent data sources are those of
/// \p primSource, so the box is placed, shown and picked like the prim it stands for.
FVP_API
PXR_NS::HdContainerDataSourceHandle CreateBoundsCurvesPrimDataSource(
    const PXR_NS::HdContainerDataSourceHandle& primSource,
    const PXR_NS::GfVec4f&                     wireframeColor);

/// Locator of the box points, which depend on the extent of the source prim.
FVP_API
const PXR_NS::HdDataSourceLocator& GetBoundsCurvesPointsLocator();

} // namespace FVP_NS_DEF

#endif // FVP_BOUNDS_CURVES_DATA_SOURCE_H
