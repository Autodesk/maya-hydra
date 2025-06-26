// Copyright 2025 Autodesk
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

#include "flowViewport/fvpPurposeRenderTagsForPasses.h"
#include <pxr/imaging/hd/tokens.h>

namespace FVP_NS_DEF {

// Apply this purpose render tag to a prim for it to be
// rendered in the secondary graphics pass, if it exists. 
// As, please note that passes can be merged in
// a single pass if the render delegates from all passes are the same.
// 
// So use Fvp::secondaryGraphicsRenderTagToken as the purpose render tag for the secondary graphics
// primitives.
const PXR_NS::TfToken secondaryGraphicsRenderTagToken = PXR_NS::TfToken("secondaryGraphics");//Use a custom render tag

} // namespace FVP_NS_DEF

