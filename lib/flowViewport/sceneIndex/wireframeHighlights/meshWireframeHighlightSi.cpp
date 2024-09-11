#include "meshWireframeHighlightSi.h"
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

HdSceneIndexBaseRefPtr MeshWireframeHighlightSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& meshPrimPath)
{
    return TfCreateRefPtr(new MeshWireframeHighlightSceneIndex(inputSceneIndex, meshPrimPath));
}

HdSceneIndexPrim MeshWireframeHighlightSceneIndex::GetPrim(const SdfPath &primPath) const
{
    if (primPath == SdfPath(_meshPrimPath.GetElementToken())) {
        return GetInputSceneIndex()->GetPrim(_meshPrimPath);
    }
    return {};
};

SdfPathVector MeshWireframeHighlightSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    if (primPath == SdfPath::AbsoluteRootPath()) {
        return {SdfPath(_meshPrimPath.GetElementToken())};
    }
    return {};
}

MeshWireframeHighlightSceneIndex::MeshWireframeHighlightSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& meshPrimPath
) : HdSingleInputFilteringSceneIndexBase(inputSceneIndex),
    InputSceneIndexUtils(inputSceneIndex),
    _meshPrimPath(meshPrimPath)
{

}

void MeshWireframeHighlightSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    // no-op
}

void MeshWireframeHighlightSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    for (const auto& entry : entries) {
        if (entry.primPath == _meshPrimPath) {
            _SendPrimsRemoved({HdSceneIndexObserver::RemovedPrimEntry(SdfPath(_meshPrimPath.GetElementToken()))});
        }
    }
}

void MeshWireframeHighlightSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    for (const auto& entry : entries) {
        if (entry.primPath == _meshPrimPath) {
            _SendPrimsDirtied({HdSceneIndexObserver::DirtiedPrimEntry(SdfPath(_meshPrimPath.GetElementToken()), entry.dirtyLocators)});
        }
    }
}

}
