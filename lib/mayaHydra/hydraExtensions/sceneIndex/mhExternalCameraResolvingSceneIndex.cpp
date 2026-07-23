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

#include "mhExternalCameraResolvingSceneIndex.h"

#include <ufeExtensions/Global.h>

#include <flowViewport/selection/fvpPathMapperRegistry.h>

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/renderProductSchema.h>
#include <pxr/imaging/hd/renderSettingsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/path.h>

#include <ufe/path.h>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace UfeExtensions;

namespace {

const TfToken kExternalCameraComponent("__adskUsd__externalCamera");
const std::string kUfeSegmentSentinel("__ufeSegment__");

Ufe::PathSegment MayaAppPathToUfePathSegment(const SdfPath& mayaAppPath)
{
    Ufe::PathSegment::Components components;
    components.push_back(Ufe::PathComponent("world"));
    for (const SdfPath& prefix : mayaAppPath.GetPrefixes()) {
        if (!prefix.IsAbsoluteRootPath()) {
            components.push_back(prefix.GetNameToken().GetString());
        }
    }
    return Ufe::PathSegment(std::move(components), getMayaRunTimeId(), '|');
}

SdfPath ResolveExternalCameraPath(const SdfPath& inputPath)
{
    SdfPath appPath; // Application path to the external camera, in SdfPath form.

    // Strip away all components up to and including the sentinel
    // component. The output of this process is an application Ufe::Path,
    // represented in SdfPath form.
    for (const SdfPath& prefix : inputPath.GetPrefixes()) {
        if (prefix.GetNameToken() == kExternalCameraComponent) {
            appPath = inputPath.ReplacePrefix(prefix, SdfPath::AbsoluteRootPath());
            break;
        }
    }
    if (appPath.IsEmpty()) {
        return inputPath;
    }

    // We are within a call to GetPrim(), which must be thread safe.
    //
    // Use the path mapper to map the app path to the actual Hydra camera path.
    // At this point the camera app path is represented as an SdfPath, with each
    // SdfPath component corresponding to a Ufe::PathComponent.
    // 
    // Convert the SdfPath to a Ufe::Path, for path mapper lookup.  Ufe::Path
    // construction is NOT thread safe, because Ufe::PathComponent creation is
    // not thread safe (insertion into the Ufe::PathComponent StringTable is
    // not thread safe).  However, because data access through the Maya API is
    // not thread safe, we already restrict Hydra batch rendering to be single
    // threaded (HYDRA-1919), and no Ufe::Path creation occurs in the batch
    // rendering main thread, so thread safety during batch rendering is not a
    // concern at time of writing (2026-05-06).
    //
    // If we remove the Hydra batch rendering single threaded restriction, a
    // way to guarantee thread safety in the following code is to create the
    // Ufe::PathComponent's for each camera Ufe::Path's ahead of time, so that
    // at GetPrim() call time, no Ufe::PathComponent creation occurs.  This
    // would be done in the following way, and only for the chosen USD render
    // settings prim, as cameras in render settings that are not used do not
    // need to be resolved.
    //
    // Before rendering (i.e. in a single threaded execution context), we
    // inspect the render settings prim and all render product prims for the
    // presence of an external camera, described as a UFE path string.  For any
    // such camera, simply convert the UFE path string to a Ufe::Path, which
    // will insert any missing Ufe::PathComponent into the Ufe::PathComponent
    // StringTable.  As no entry can be removed from the StringTable, this will
    // guarantee the existence of all Ufe::PathComponent's for the chosen
    // external cameras.
    //
    // An obvious other alternative is to make the Ufe::PathComponent
    // StringTable thread safe in a future UFE version (with single writer,
    // multiple reader behavior), which has been considered in the past, but
    // this requires UFE versus Maya TBB configuration management.

    Ufe::Path appUfePath;
    SdfPath  mayaAppPath;
    SdfPath  extCamPath;
    Ufe::Rtid rtId;
    for (const SdfPath& prefix : appPath.GetPrefixes()) {
        const std::string& name = prefix.GetNameToken().GetString();
        if (TfStringStartsWith(name, kUfeSegmentSentinel)) {
            const std::string rtIdStr = name.substr(kUfeSegmentSentinel.size());
            rtId = static_cast<Ufe::Rtid>(std::stoul(rtIdStr));
            mayaAppPath = prefix.GetParentPath();
            extCamPath = appPath.ReplacePrefix(prefix, SdfPath::AbsoluteRootPath());
            break;  // We assume only 2 segments.
        }
    }

    if (!extCamPath.IsEmpty() && rtId) {
        appUfePath = Ufe::Path(Ufe::Path::Segments {
            MayaAppPathToUfePathSegment(mayaAppPath),
            sdfPathToUfePathSegment(extCamPath, rtId) });
    } else {
        appUfePath = Ufe::Path(MayaAppPathToUfePathSegment(appPath));
    }
    auto hydraPath = Fvp::ufePathToPrimSelections(appUfePath);

    // Camera is non-instanced, so there will be a single PrimSelection.
    if (!TF_VERIFY(hydraPath.size() == 1)) {
        return inputPath;
    }
    return hydraPath[0].primPath;
}

/// Wraps a single render product container, overriding cameraPrim when it
/// contains the __adskUsd__externalCamera sentinel component.
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
        if (name == HdRenderProductSchemaTokens->cameraPrim) {
            auto cameraPrimDs = HdTypedSampledDataSource<SdfPath>::Cast(
                _input->Get(HdRenderProductSchemaTokens->cameraPrim));
            if (cameraPrimDs) {
                const SdfPath resolved =
                    ResolveExternalCameraPath(cameraPrimDs->GetTypedValue(0));
                if (resolved != cameraPrimDs->GetTypedValue(0)) {
                    return HdRetainedTypedSampledDataSource<SdfPath>::New(resolved);
                }
            }
        }
        return _input->Get(name);
    }

private:
    _ResolvedRenderProductDataSource(
        const HdContainerDataSourceHandle& input)
        : _input(input)
    {
    }

    HdContainerDataSourceHandle _input;
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
            return _ResolvedRenderProductDataSource::New(childContainer);
        }
        return child;
    }

private:
    _ResolvedRenderProductsDataSource(
        const HdVectorDataSourceHandle& input)
        : _input(input)
    {
    }

    HdVectorDataSourceHandle _input;
};

} // anonymous namespace

namespace MAYAHYDRA_NS_DEF {

ExternalCameraResolvingSceneIndexRefPtr
ExternalCameraResolvingSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
    return TfCreateRefPtr(
        new ExternalCameraResolvingSceneIndex(inputSceneIndex));
}

ExternalCameraResolvingSceneIndex::ExternalCameraResolvingSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
{
}

HdSceneIndexPrim
ExternalCameraResolvingSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    // If prim isn't a Hydra render settings prim, return it unchanged.
    if (!prim.dataSource
        || prim.primType != HdPrimTypeTokens->renderSettings) {
        return prim;
    }

    auto renderSettingsDs = HdContainerDataSource::Cast(
        prim.dataSource->Get(HdRenderSettingsSchemaTokens->renderSettings));
    if (!renderSettingsDs) {
        return prim;
    }

    // If the render settings prim doesn't have render products, return it
    // unchanged.
    auto renderProductsDs = HdVectorDataSource::Cast(
        renderSettingsDs->Get(HdRenderSettingsSchemaTokens->renderProducts));
    if (!renderProductsDs) {
        return prim;
    }

    HdDataSourceLocator productsLocator(
        HdRenderSettingsSchemaTokens->renderSettings,
        HdRenderSettingsSchemaTokens->renderProducts);

    // Resolve external cameras for all render products.
    prim.dataSource = HdContainerDataSourceEditor(prim.dataSource)
        .Set(productsLocator,
             _ResolvedRenderProductsDataSource::New(renderProductsDs))
        .Finish();

    return prim;
}

void ExternalCameraResolvingSceneIndex::_PrimsAdded(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    if (!_IsObserved()) return;
    _SendPrimsAdded(entries);
}

void ExternalCameraResolvingSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    if (!_IsObserved()) return;
    _SendPrimsRemoved(entries);
}

void ExternalCameraResolvingSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    if (!_IsObserved()) return;
    _SendPrimsDirtied(entries);
}

} // namespace MAYAHYDRA_NS_DEF
