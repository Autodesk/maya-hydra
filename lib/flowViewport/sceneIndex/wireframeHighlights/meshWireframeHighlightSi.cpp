#include "meshWireframeHighlightSi.h"
#include "baseWireframeHighlightSi.h"
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <iostream>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

HdSceneIndexBaseRefPtr MeshWireframeHighlightSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& meshPrimPath)
{
    return TfCreateRefPtr(new MeshWireframeHighlightSceneIndex(inputSceneIndex, meshPrimPath));
}

SdfPath MeshWireframeHighlightSceneIndex::_MeshNamePath() const
{
    return SdfPath::AbsoluteRootPath().AppendChild(_meshPrimPath.GetNameToken());
}

HdSceneIndexPrim MeshWireframeHighlightSceneIndex::GetPrim(const SdfPath &primPath) const
{
    std::cout << "MeshWireframeHighlightSceneIndex::GetPrim for " << primPath.GetString() << std::endl;
    if (primPath.HasPrefix(_MeshNamePath())) {
        std::cout << "Returning prim" << std::endl;
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath.ReplacePrefix(_MeshNamePath(), _meshPrimPath));
        prim.dataSource = MakeWireframe(prim.dataSource, GfVec4f(1.0, 0, 0, 1.0));
        return prim;
    }
    return {};
};

SdfPathVector MeshWireframeHighlightSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    std::cout << "MeshWireframeHighlightSceneIndex::GetChildPrimPaths for " << primPath.GetString() << std::endl;
    if (primPath == SdfPath::AbsoluteRootPath()) {
        std::cout << "MeshWireframeHighlightSceneIndex::GetChildPrimPaths : " << _MeshNamePath().GetString() << std::endl;
        return {_MeshNamePath()};
    } else if (primPath.HasPrefix(_MeshNamePath())) {
        auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath.ReplacePrefix(_MeshNamePath(), _meshPrimPath));
        for (auto& childPath : childPaths) {
            childPath = childPath.ReplacePrefix(_meshPrimPath, _MeshNamePath());
        }
        std::cout << "MeshWireframeHighlightSceneIndex::GetChildPrimPaths : " << std::endl;
        for (const auto& childPath : childPaths) {
            std::cout << childPath.GetString() << std::endl;
        }
        return childPaths;
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
            _SendPrimsRemoved({HdSceneIndexObserver::RemovedPrimEntry(_MeshNamePath())});
        }
    }
}

void MeshWireframeHighlightSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    for (const auto& entry : entries) {
        if (entry.primPath == _meshPrimPath) {
            _SendPrimsDirtied({HdSceneIndexObserver::DirtiedPrimEntry(_MeshNamePath(), entry.dirtyLocators)});
        }
    }
}

}
