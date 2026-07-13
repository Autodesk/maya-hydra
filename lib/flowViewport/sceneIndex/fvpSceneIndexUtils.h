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
#ifndef FVP_SCENE_INDEX_UTILS_H
#define FVP_SCENE_INDEX_UTILS_H

#include <flowViewport/api.h>

#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/sceneIndex.h>

#include <functional>
#include <ostream>
#include <string>
#include <vector>

namespace FVP_NS_DEF {

/// \class InputSceneIndexUtils
///
/// Utility class to factor out common single input scene index functionality.
///
template<class T>
class InputSceneIndexUtils
{
public:

    InputSceneIndexUtils(
        const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex
    ) 
#ifdef CODE_COVERAGE_WORKAROUND
        : _inputSceneIndex(inputSceneIndex)
#endif
    {}

    // At time of writing directly accessing _GetInputSceneIndex() from
    // clang coverage build causes crash.  PPT, 24-Jan-2024.
    const PXR_NS::HdSceneIndexBaseRefPtr& GetInputSceneIndex() const
    {
#ifdef CODE_COVERAGE_WORKAROUND
        return _inputSceneIndex;
#else
        return static_cast<const T*>(this)->_GetInputSceneIndex();
#endif
    }

private:

#ifdef CODE_COVERAGE_WORKAROUND
    const PXR_NS::HdSceneIndexBaseRefPtr _inputSceneIndex;
#endif
};

// ============================================================================
// SceneIndexInspector and associated types
// ============================================================================

struct FVP_API PrimEntry
{
    PXR_NS::SdfPath          primPath;
    PXR_NS::HdSceneIndexPrim prim;
};

using FindPrimPredicate
    = std::function<bool(const PXR_NS::HdSceneIndexBasePtr& sceneIndex, const PXR_NS::SdfPath& primPath)>;

using PrimEntriesVector = std::vector<PrimEntry>;

/// \class SceneIndexInspector
///
/// Utility class for inspecting a Hydra scene index hierarchy.  Provides
/// methods to search for prims by predicate and to dump the full hierarchy
/// (including data sources) as a text tree.
///
class FVP_API SceneIndexInspector
{
public:
    SceneIndexInspector(PXR_NS::HdSceneIndexBasePtr sceneIndex);
    ~SceneIndexInspector();

    /// Retrieve the underlying scene index of this inspector.
    PXR_NS::HdSceneIndexBasePtr GetSceneIndex();

    /**
     * @brief Retrieve all prims that match the given predicate, up until the maximum amount
     *
     * A maximum amount of 0 means unlimited (all matching prims will be returned).
     *
     * @param[in] predicate is the callable predicate used to determine whether a given prim is
     * desired
     * @param[in] maxPrims is the maximum amount of prims to be retrieved. The default value is 0
     * (unlimited).
     *
     * @return A vector of the prim entries that matched the given predicate.
     */
    PrimEntriesVector FindPrims(FindPrimPredicate predicate, size_t maxPrims = 0) const;

    /**
     * @brief Print the scene index's hierarchy in a tree-like format
     *
     * Print the scene index's hierarchy in a tree-like format, down to the individual data
     * source level.
     *
     * @param[out] outStream is the stream in which to print the hierarchy
     */
    void WriteHierarchy(std::ostream& outStream) const;

private:
    void _FindPrims(
        FindPrimPredicate  predicate,
        const PXR_NS::SdfPath&     primPath,
        PrimEntriesVector& primEntries,
        size_t             maxPrims) const;

    void _WritePrimHierarchy(
        PXR_NS::SdfPath   primPath,
        std::string        selfPrefix,
        std::string        childrenPrefix,
        std::ostream&      outStream) const;

    void _WriteContainerDataSource(
        PXR_NS::HdContainerDataSourceHandle dataSource,
        std::string                         dataSourceName,
        std::string                         selfPrefix,
        std::string                         childrenPrefix,
        std::ostream&                       outStream) const;

    void _WriteVectorDataSource(
        PXR_NS::HdVectorDataSourceHandle dataSource,
        std::string                      dataSourceName,
        std::string                      selfPrefix,
        std::string                      childrenPrefix,
        std::ostream&                    outStream) const;

    void _WriteLeafDataSource(
        PXR_NS::HdDataSourceBaseHandle dataSource,
        std::string                    dataSourceName,
        std::string                    selfPrefix,
        std::ostream&                  outStream) const;

    PXR_NS::HdSceneIndexBasePtr _sceneIndex;
};

// ============================================================================
// Scene index tree search utilities
// ============================================================================

/// \class SceneIndexDisplayNamePred
///
/// Predicate to match a scene index by its display name string.
///
class FVP_API SceneIndexDisplayNamePred {
    const std::string _name;
public:
    SceneIndexDisplayNamePred(const std::string& name) : _name(name) {}

    bool operator()(const PXR_NS::HdSceneIndexBaseRefPtr& sceneIndex) {
        return sceneIndex->GetDisplayName() == _name;
    }
};

/// Find the first scene index matching the given predicate via depth-first
/// search of the scene index tree.
///
/// \param sceneIndex  The root of the scene index tree to search.
/// \param predicate   The predicate that determines a match.
/// \return Scene index pointer if the predicate succeeds, otherwise nullptr.
FVP_API
PXR_NS::HdSceneIndexBaseRefPtr findSceneIndexInTree(
    const PXR_NS::HdSceneIndexBaseRefPtr&                             sceneIndex,
    const std::function<bool(const PXR_NS::HdSceneIndexBaseRefPtr&)>& predicate
);

}

#endif
