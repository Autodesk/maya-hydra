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

#include "mhGenerativeProceduralResolvingSceneIndex.h"

#include "flowViewport/tokens.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/visibilitySchema.h>

#include <algorithm>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

MAYAHYDRALIB_API
MhGenerativeProceduralResolvingSceneIndexRefPtr
MhGenerativeProceduralResolvingSceneIndex::New(const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
#if PXR_VERSION < 2511
    // Windows fails to link mayaHydraLib against shared usd_hdGp here: the import
    // library exposes only exported DLL symbols. HdGpGenerativeProceduralResolvingSceneIndex::New()
    // needs ctor symbols that are not exported before USD 25.11;
    return TfCreateRefPtr(new MhGenerativeProceduralResolvingSceneIndex(inputSceneIndex));
#else
    return TfCreateRefPtr(new MhGenerativeProceduralResolvingSceneIndex(
        HdGpGenerativeProceduralResolvingSceneIndex::New(inputSceneIndex)));
#endif

}

// Composes the parent procedural's world matrix into generated children's transform, 
// so the generated geometry moves correctly when the procedural root is transformed.
// Returns the modified prim with the overlaid world-space transform.
HdSceneIndexPrim MhGenerativeProceduralResolvingSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    if (_generativeProceduralPaths.empty()) {
        return prim;
    }

    // Walk up from the prim to find the nearest GP root ancestor.
    SdfPath generativeProceduralRoot;
    {
        SdfPath parentPath = primPath.GetParentPath();
        while (!parentPath.IsEmpty() && !parentPath.IsAbsoluteRootPath()) {
            if (_generativeProceduralPaths.count(parentPath)) {
                generativeProceduralRoot = parentPath;
                break;
            }
            parentPath = parentPath.GetParentPath();
        }
    }
    if (generativeProceduralRoot.IsEmpty())
        return prim;

    // Compose transforms from the prim up through all ancestors to the GP root.
    GfMatrix4d result(1.0);
    SdfPath path = primPath;
    bool isVisible = true;
    while (path != generativeProceduralRoot.GetParentPath() && !path.IsEmpty() && !path.IsAbsoluteRootPath()) {
        const HdSceneIndexPrim ancestorPrim = (path == primPath) ? prim : GetInputSceneIndex()->GetPrim(path);
        if (ancestorPrim.dataSource) {
            if (auto matDataSource
                = HdXformSchema::GetFromParent(ancestorPrim.dataSource).GetMatrix()) {
                result = result * matDataSource->GetTypedValue(0);
            }
            auto visibilityDataSource
                = HdVisibilitySchema::GetFromParent(ancestorPrim.dataSource).GetVisibility();
            if (visibilityDataSource && !visibilityDataSource->GetTypedValue(0)) {
                isVisible = false;
            }
        }
        path = path.GetParentPath();
    }

    prim.dataSource = HdOverlayContainerDataSource::New(
        HdRetainedContainerDataSource::New(
            HdVisibilitySchemaTokens->visibility,
            HdVisibilitySchema::Builder()
                .SetVisibility(HdRetainedTypedSampledDataSource<bool>::New(isVisible))
                .Build(),
            HdXformSchemaTokens->xform,
            HdXformSchema::Builder()
                .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(result))
                .Build()),
        prim.dataSource);

    return prim;
}

void MhGenerativeProceduralResolvingSceneIndex::_PrimsAdded(
    const PXR_NS::HdSceneIndexBase&                       sender,
    const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries)
{
    if (!_IsObserved())
        return;
    // Cache primPaths of GP roots to reduce GetPrim calls 
    for (const auto& entry : entries) {
        if (entry.primType == FvpGenerativeProceduralTokens->resolvedHydraGenerativeProcedural
            || entry.primType == FvpGenerativeProceduralTokens->hydraGenerativeProcedural) {
            _generativeProceduralPaths.insert(entry.primPath);
        }
    }
    _SendPrimsAdded(entries);
}

void MhGenerativeProceduralResolvingSceneIndex::_PrimsRemoved(
    const PXR_NS::HdSceneIndexBase&                         sender,
    const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    if (!_IsObserved())
        return;
    for (const auto& entry : entries) {
        // Remove generative procedural entries on the prim and its children
        for (auto it = _generativeProceduralPaths.begin();
             it != _generativeProceduralPaths.end();) {
            if ((*it).HasPrefix(entry.primPath)) {
                it = _generativeProceduralPaths.erase(it);
            } else {
                ++it;
            }
        }
    }
    _SendPrimsRemoved(entries);
}

// Forwards dirty notifications to generated meshes.
// Allows the generated meshes to move when there is a transformed applied
// to the root. 
void MhGenerativeProceduralResolvingSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    if (!_IsObserved())
        return;

    // No generative procedurals registered -> pass through.
    if (_generativeProceduralPaths.empty()) {
        _SendPrimsDirtied(entries);
        return;
    }

    static const HdDataSourceLocatorSet xformLocatorSet { HdXformSchema::GetDefaultLocator() };
    static const HdDataSourceLocatorSet visibilityLocatorSet { HdVisibilitySchema::GetDefaultLocator() };
    static const HdDataSourceLocator    xformLocator      = HdXformSchema::GetDefaultLocator();
    static const HdDataSourceLocator    visibilityLocator = HdVisibilitySchema::GetDefaultLocator();

    HdSceneIndexObserver::DirtiedPrimEntries expandedEntries;
    expandedEntries.reserve(entries.size());

    for (const auto& entry : entries) {
        expandedEntries.push_back(entry);

        if (_generativeProceduralPaths.find(entry.primPath) == _generativeProceduralPaths.end())
            continue;

        if (entry.dirtyLocators.Intersects(xformLocator))
            _DirtyDescendantsLocator(entry.primPath, xformLocatorSet, expandedEntries);

        if (entry.dirtyLocators.Intersects(visibilityLocator))
            _DirtyDescendantsLocator(entry.primPath, visibilityLocatorSet, expandedEntries);
    }

    _SendPrimsDirtied(expandedEntries);
}

void MhGenerativeProceduralResolvingSceneIndex::_DirtyDescendantsLocator(
    const SdfPath&                            path,
    const HdDataSourceLocatorSet&             locators,
    HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    for (const auto& childPath : GetInputSceneIndex()->GetChildPrimPaths(path)) {
        entries.emplace_back(childPath, locators);
        _DirtyDescendantsLocator(childPath, locators, entries);
    }
}

} // namespace MAYAHYDRA_NS_DEF