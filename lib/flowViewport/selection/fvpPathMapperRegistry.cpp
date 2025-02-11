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

#include <flowViewport/selection/fvpPathMapperRegistry.h>
#include <flowViewport/selection/fvpPathMapper.h>

#include <pxr/base/tf/instantiateSingleton.h>

#include <ufe/path.h>
#include <ufe/pathString.h>
#include <ufe/trie.imp.h>

namespace {

Ufe::Trie<Fvp::PathMapperConstPtr> mappers;
Fvp::PathMapperConstPtr fallbackMapper{};

}

PXR_NAMESPACE_OPEN_SCOPE
TF_INSTANTIATE_SINGLETON(Fvp::PathMapperRegistry);
PXR_NAMESPACE_CLOSE_SCOPE

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

/* static */
PathMapperRegistry& PathMapperRegistry::Instance()
{
    return PXR_NS::TfSingleton<PathMapperRegistry>::GetInstance();
}

bool PathMapperRegistry::Register(const Ufe::Path& prefix, const PathMapperConstPtr& pathMapper)
{
    if (prefix.empty() || mappers.containsDescendantInclusive(prefix) || 
        mappers.containsAncestor(prefix)) {
        return false;
    }

    mappers.add(prefix, pathMapper);
    return true;
}

bool PathMapperRegistry::Unregister(const Ufe::Path& prefix)
{
    return mappers.remove(prefix) != nullptr;
}

bool PathMapperRegistry::Update(const Ufe::Path& oldPrefix, const Ufe::Path& newPrefix)
{
    auto mapper = GetMapper(oldPrefix);
    if (!mapper) {
        return false;
    }
    TF_AXIOM(Unregister(oldPrefix));
    TF_AXIOM(Register(newPrefix, mapper));
    return true;
}

void PathMapperRegistry::SetFallbackMapper(
    const PathMapperConstPtr& pathMapper
)
{
    fallbackMapper = pathMapper;
}

PathMapperConstPtr PathMapperRegistry::GetFallbackMapper() const
{
    return fallbackMapper;
}

PathMapperConstPtr PathMapperRegistry::_GetMapper(const Ufe::Path& path) const
{
    TF_AXIOM(!path.empty());

    if (mappers.empty()) {
        return nullptr;
    }

    // We are looking for the closest ancestor of the argument.  Internal trie
    // nodes have no data, and exist only as parents for trie nodes with data.
    // In our case the trie node data is the path mapper, so we walk down the 
    // path trying to find a trie node with data.
    auto trieNode = mappers.root();
    for (const auto& c : path) {
        auto child = (*trieNode)[c];
        // If we've reached a trie leaf node before the end of our path, there
        // is no trie node with data as ancestor of the path.
        if (!child) {
            return nullptr;
        }
        trieNode = child;

        // Found a trieNode with data.
        if (trieNode->hasData()) {
            return trieNode->data();
        }
    }
    // We reached the end of the parent path without returning true, therefore
    // there are no ancestors.
    return nullptr;
}

PathMapperConstPtr PathMapperRegistry::GetMapper(const Ufe::Path& path) const
{
    if (path.empty()) {
        return nullptr;
    }

    auto mapper = _GetMapper(path);
    return mapper ? mapper : fallbackMapper;
}

PrimSelections PathMapperRegistry::UfePathToPrimSelections(
    const Ufe::Path& appPath
) const
{
    if (appPath.empty()) {
        return {};
    }

    PrimSelections primSelections;

    auto mapper = GetMapper(appPath);
        
    if (!mapper) {
        TF_WARN("No registered mapping for path %s, no prim path returned.", Ufe::PathString::string(appPath).c_str());
    }
    else {
        primSelections = mapper->UfePathToPrimSelections(appPath);
        if (primSelections.empty()) {

            mapper = GetFallbackMapper();
        
            if (!mapper) {
                TF_WARN("No registered fallback mapping, no prim path returned.");
            }
            else {
                primSelections = mapper->UfePathToPrimSelections(appPath);
                if (primSelections.empty()) {
                    TF_WARN("Mapping for path %s returned no prim path.", Ufe::PathString::string(appPath).c_str());
                }
            }
        }
    }

    return primSelections;
}

bool PathMapperRegistry::HasMapper(const Ufe::Path& path) const
{
    if (path.empty()) {
        return false;
    }
    return _GetMapper(path) != nullptr;
}

PrimSelections ufePathToPrimSelections(const Ufe::Path& appPath)
{
    return PathMapperRegistry::Instance().UfePathToPrimSelections(appPath);
}

SdfPathVector sceneIndexPaths(const Ufe::Path& appPath)
{
    auto primSelections = ufePathToPrimSelections(appPath);

    SdfPathVector outVector;
    outVector.reserve(primSelections.size());
    for (const auto& primSelection : primSelections) {
        outVector.emplace_back(primSelection.primPath);
    }
    return outVector;
}

SdfPath sceneIndexPath(const Ufe::Path& appPath)
{
    auto primSelections = ufePathToPrimSelections(appPath);
    if (!TF_VERIFY(primSelections.size() <= 2u)) {
        throw PrimPathsCountOutOfRangeException(0, 2, primSelections.size());
    }
    return primSelections.empty() ? SdfPath() : primSelections.front().primPath;
}

}
