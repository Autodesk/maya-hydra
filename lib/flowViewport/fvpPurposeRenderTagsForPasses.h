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
#ifndef FVP_PURPOSE_RENDER_TAGS_FOR_PASSES_H
#define FVP_PURPOSE_RENDER_TAGS_FOR_PASSES_H

#include <flowViewport/api.h>
#include <pxr/base/tf/token.h>

namespace FVP_NS_DEF {

/** Apply this purpose render tag to a hydra primitive to be
    rendered in the secondary graphic pass. Please note that passes can be merged in
    a single pass if the render delegates from all passes are the same.
*/
FVP_API
extern const PXR_NS::TfToken secondaryGraphicsRenderTagToken;

} // namespace FVP_NS_DEF

#endif // FVP_PURPOSE_RENDER_TAGS_FOR_PASSES_H
