//
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

#include "fvpFrameNbResolvingSceneIndex.h"

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/renderProductSchema.h>
#include <pxr/imaging/hd/renderSettingsSchema.h>
#include <pxr/imaging/hd/sceneGlobalsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <cmath>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

std::string resolveFrameNumber(const std::string& input, double currentFrame)
{
    const int frameInt = static_cast<int>(std::max(0.0, std::round(currentFrame)));
    const std::string frameStr = std::to_string(frameInt);

    std::string result;
    result.reserve(input.size());

    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '#') {
            size_t runStart = i;
            while (i < input.size() && input[i] == '#') {
                ++i;
            }
            const size_t runLen = i - runStart;

            if (frameStr.size() < runLen) {
                // Zero-pad on the left to reach the minimum field width.
                result.append(runLen - frameStr.size(), '0');
            }
            result.append(frameStr);
        } else {
            result.push_back(input[i]);
            ++i;
        }
    }

    return result;
}

/// Wraps a single render product container, overriding the name data source
/// to resolve '#' hash-mark patterns with the current frame number.
class _ResolvedRenderProductDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_ResolvedRenderProductDataSource);

    TfTokenVector GetNames() override
    {
        return _input->GetNames();
    }

    HdDataSourceBaseHandle Get(const TfToken& name) override
    {
        if (name == HdRenderProductSchemaTokens->name) {
            auto nameDs = HdTypedSampledDataSource<TfToken>::Cast(
                _input->Get(HdRenderProductSchemaTokens->name));
            if (nameDs) {
                const std::string nameStr = nameDs->GetTypedValue(0).GetString();
                const std::string resolved = resolveFrameNumber(nameStr, _currentFrame);
                if (resolved != nameStr) {
                    return HdRetainedTypedSampledDataSource<TfToken>::New(
                        TfToken(resolved));
                }
            }
        }
        return _input->Get(name);
    }

private:
    _ResolvedRenderProductDataSource(
        const HdContainerDataSourceHandle& input,
        double currentFrame)
        : _input(input)
        , _currentFrame(currentFrame)
    {
    }

    HdContainerDataSourceHandle _input;
    double _currentFrame;
};

/// Wraps the renderProducts vector, returning resolved wrappers for each
/// element.
class _ResolvedRenderProductsDataSource : public HdVectorDataSource
{
public:
    HD_DECLARE_DATASOURCE(_ResolvedRenderProductsDataSource);

    size_t GetNumElements() override
    {
        return _input->GetNumElements();
    }

    HdDataSourceBaseHandle GetElement(size_t element) override
    {
        auto child = _input->GetElement(element);
        if (auto childContainer = HdContainerDataSource::Cast(child)) {
            return _ResolvedRenderProductDataSource::New(
                childContainer, _currentFrame);
        }
        return child;
    }

private:
    _ResolvedRenderProductsDataSource(
        const HdVectorDataSourceHandle& input,
        double currentFrame)
        : _input(input)
        , _currentFrame(currentFrame)
    {
    }

    HdVectorDataSourceHandle _input;
    double _currentFrame;
};

} // anonymous namespace

namespace FVP_NS_DEF {

FrameNbResolvingSceneIndexRefPtr
FrameNbResolvingSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
    return TfCreateRefPtr(
        new FrameNbResolvingSceneIndex(inputSceneIndex));
}

FrameNbResolvingSceneIndex::FrameNbResolvingSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
{
}

HdSceneIndexPrim
FrameNbResolvingSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    auto sceneGlobals = HdSceneGlobalsSchema::GetFromSceneIndex(
        GetInputSceneIndex());
    if (!sceneGlobals.IsDefined()) {
        return prim;
    }

    if (!prim.dataSource) {
        return prim;
    }

    // Is the argument prim the active render settings prim?  If so, perform
    // render product frame number resolution.  If not, nothing to do.
    auto activeRenderSettingsDs = sceneGlobals.GetActiveRenderSettingsPrim();
    if (!activeRenderSettingsDs
        || activeRenderSettingsDs->GetTypedValue(0) != primPath) {
        return prim;
    }

    auto renderSettingsDs = HdContainerDataSource::Cast(
        prim.dataSource->Get(HdRenderSettingsSchemaTokens->renderSettings));
    if (!renderSettingsDs) {
        return prim;
    }

    auto renderProductsDs = HdVectorDataSource::Cast(
        renderSettingsDs->Get(HdRenderSettingsSchemaTokens->renderProducts));
    if (!renderProductsDs) {
        return prim;
    }

    auto currentFrameDs = sceneGlobals.GetCurrentFrame();
    if (!currentFrameDs) {
        return prim;
    }
    const double currentFrame = currentFrameDs->GetTypedValue(0);

    HdDataSourceLocator productsLocator(
        HdRenderSettingsSchemaTokens->renderSettings,
        HdRenderSettingsSchemaTokens->renderProducts);

    prim.dataSource = HdContainerDataSourceEditor(prim.dataSource)
        .Set(productsLocator,
             _ResolvedRenderProductsDataSource::New(
                 renderProductsDs, currentFrame))
        .Finish();

    return prim;
}

void FrameNbResolvingSceneIndex::_PrimsAdded(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    if (!_IsObserved()) return;
    _SendPrimsAdded(entries);
}

void FrameNbResolvingSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    if (!_IsObserved()) return;
    _SendPrimsRemoved(entries);
}

void FrameNbResolvingSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    if (!_IsObserved()) return;

    // When time changes we dirty render product names on the active render
    // settings prim.  We do this unconditionally, regardless of whether or not
    // they have '#' sequences that need resolution.  The rationale is that in
    // a single frame render there is no time changes, and for a multi-frame
    // render, render product names were likely authored with '#' sequences.
    // The alternative is time sampled render product names without '#'
    // sequences, but these are very awkward to author in USD, so even if that
    // is the case the only down side is unnecessary dirtying.
    //
    // We observe time change through a dirty notification on the scene globals
    // current frame, then dirty the render products on the active render
    // settings prim.
    //
    // This follows the established Hydra scene index contract that a filtering
    // scene index that transforms data is responsible for propagating dirty
    // state for the data it transforms. It's self-contained and requires no
    // API additions to this class.
    // 
    // The down side is that it is O(n) on the number of input dirty entries
    // (but n is usually small), and requires the FrameNbResolvingSceneIndex to
    // be downstream of the scene globals scene index.
    //
    HdSceneIndexObserver::DirtiedPrimEntries augmented(entries);
    for (const auto& entry : entries) {
        if (entry.primPath == SdfPath::AbsoluteRootPath()
            && entry.dirtyLocators.Intersects(
                   HdSceneGlobalsSchema::GetCurrentFrameLocator())) {
            auto sceneGlobals = HdSceneGlobalsSchema::GetFromSceneIndex(
                GetInputSceneIndex());
            if (auto arsp = sceneGlobals.GetActiveRenderSettingsPrim()) {
                augmented.emplace_back(
                    arsp->GetTypedValue(0),
                    HdRenderSettingsSchema::GetRenderProductsLocator());
            }
            break;
        }
    }
    _SendPrimsDirtied(augmented);
}

} // namespace FVP_NS_DEF
