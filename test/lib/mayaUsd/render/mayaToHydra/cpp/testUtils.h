//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

#ifndef MAYAHYDRA_TEST_UTILS_H
#define MAYAHYDRA_TEST_UTILS_H

#include <mayaHydraLib/mayaHydra.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/visibilitySchema.h>

#include <maya/MMatrix.h>
#include <maya/MStatus.h>

#include <fstream>
#include <functional>
#include <limits>

PXR_NAMESPACE_OPEN_SCOPE

constexpr double DEFAULT_TOLERANCE = std::numeric_limits<double>::epsilon();

using SceneIndicesVector = std::vector<HdSceneIndexBaseRefPtr>;

/**
 * @brief Retrieve the list of registered terminal scene indices from the Hydra plugin
 *
 * @return A reference to the vector of registered terminal scene indices.
 */
const SceneIndicesVector& GetTerminalSceneIndices();

/**
 * @brief Compare a Hydra and a Maya matrix and return whether they are similar
 *
 * Compare a Hydra and a Maya matrix and return whether the difference between each of their
 * corresponding elements is less than or equal to the given tolerance.
 *
 * @param[in] hydraMatrix is the Hydra matrix
 * @param[in] mayaMatrix is the Maya matrix
 * @param[in] tolerance is the maximum allowed difference between two corresponding elements of the
 * matrices. The default value is the epsilon for double precision floating-point numbers.
 *
 * @return True if the two matrices are similar enough given the tolerance, false otherwise.
 */
bool MatricesAreClose(
    const GfMatrix4d& hydraMatrix,
    const MMatrix&    mayaMatrix,
    double            tolerance = DEFAULT_TOLERANCE);

struct PrimEntry
{
    SdfPath          primPath;
    HdSceneIndexPrim prim;
};

using FindPrimPredicate
    = std::function<bool(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath)>;

using PrimEntriesVector = std::vector<PrimEntry>;

class SceneIndexInspector
{
public:
    SceneIndexInspector(HdSceneIndexBasePtr sceneIndex);
    ~SceneIndexInspector();

    /**
     * @brief Retrieve the underlying scene index of this inspector
     *
     * The returned pointer is non-owning.
     *
     * @return A pointer to the underlying scene index of this inspector.
     */
    HdSceneIndexBasePtr GetSceneIndex();

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
        const SdfPath&     primPath,
        PrimEntriesVector& primEntries,
        size_t             maxPrims) const;

    void _WritePrimHierarchy(
        SdfPath       primPath,
        std::string   selfPrefix,
        std::string   childrenPrefix,
        std::ostream& outStream) const;

    void _WriteContainerDataSource(
        HdContainerDataSourceHandle dataSource,
        std::string                 dataSourceName,
        std::string                 selfPrefix,
        std::string                 childrenPrefix,
        std::ostream&               outStream) const;

    void _WriteVectorDataSource(
        HdVectorDataSourceHandle dataSource,
        std::string              dataSourceName,
        std::string              selfPrefix,
        std::string              childrenPrefix,
        std::ostream&            outStream) const;

    void _WriteLeafDataSource(
        HdDataSourceBaseHandle dataSource,
        std::string            dataSourceName,
        std::string            selfPrefix,
        std::ostream&          outStream) const;

    HdSceneIndexBasePtr _sceneIndex;
};

class PrimNamePredicate
{
public:
    PrimNamePredicate(const std::string& primName) : _primName(primName) {}

    /**
     * @brief Predicate to match a prim name. This class is to be used as a FindPrimPredicate.
     *
     * @param[in] _ Unused scene index parameter. Is only present to conform to the 
     * FindPrimPredicate signature.
     * @param[in] primPath The prim path to test.
     *
     * @return True if the argument prim path's name matches the predicate's prim name, false otherwise.
     */
    bool operator()(const HdSceneIndexBasePtr& _, const SdfPath& primPath) {
        return primPath.GetName() == _primName;
    }

private:
    const std::string _primName;
};

/**
* @brief PrimNameVisibilityPredicate is a Predicate to find a name in a primitive SdfPath and check its visibility attribute. This class is to be used as a FindPrimPredicate.
* It returns True if both conditions : 
* 1) The predicate's prim name is found in one of the prims from the scene index (it only needs to be inside a path, not matching it exactly), 
* 2) If 1) is filled, we check if the visibility attribute is set to true.
* This Predicate returns true only if both conditions are validated. Or false if one these conditions is not filled.
*/
class PrimNameVisibilityPredicate 
{
public:
    PrimNameVisibilityPredicate(const std::string primName) : _primName(primName) {}

    /**
    * @brief Predicate to find a name in a primitive SdfPath and check its visibility attribute. This class is to be used as a FindPrimPredicate.
    *
    * @param[in] sceneIndex :  scene index to use for checking the prims .
    * @param[in] primPath : The prim path to test.
    *
    * @return True if the argument prim path's name has the predicate's prim name inside, and if the visibility attribute is set to true, false otherwise.
    */
    bool operator()(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) {
        const std::string primPathString    = primPath.GetAsString();
        HdSceneIndexPrim prim               = sceneIndex->GetPrim(primPath);
        if (primPathString.find(_primName) != std::string::npos) {
            //Check if it is visible or not
            auto visibilityHandle = HdVisibilitySchema::GetFromParent(prim.dataSource).GetVisibility();
            if (visibilityHandle){
                return visibilityHandle->GetTypedValue(0.0f); //return true if it is visible, false otherwise
            }
        }
        return false;
    }

private:
    const std::string _primName;
};

class MeshPrimPredicate
{
public:
    MeshPrimPredicate(const std::string& objectName)
        : _objectName(objectName)
    {
    }

    /**
     * @brief Predicate to match a mesh prim from the original object's name. This class is to be used as a FindPrimPredicate.
     *
     * @param[in] sceneIndex The scene index to test.
     * @param[in] primPath The prim path to test.
     *
     * @return True if the argument prim path's contains the object name, and its prim type is mesh.
     */
    bool operator()(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath)
    {
        return primPath.GetAsString().find(_objectName) != std::string::npos
            && sceneIndex->GetPrim(primPath).primType == HdPrimTypeTokens->mesh;
    }

private:
    const std::string _objectName;
};

class SceneIndexDisplayNamePred {
    const std::string _name;
public:
    SceneIndexDisplayNamePred(const std::string& name) : _name(name) {}

    /**
     * @brief Predicate to match a scene index display name string.
     *
     * @param[in] sceneIndex The scene index to test.
     *
     * @return True if the argument scene index matches the display name string, false otherwise.
     */
    bool operator()(const HdSceneIndexBaseRefPtr& sceneIndex) {
        return sceneIndex->GetDisplayName() == _name;
    }
};

/**
 * @brief Find the first scene index matching argument predicate in depth first search.
 *
 * @param[in] sceneIndex The root of the scene index tree to search.
 * @param[in] predicate The predicate that determines a match.
 *
 * @return Scene index pointer if the predicate succeeds, otherwise nullptr.
 */
HdSceneIndexBaseRefPtr findSceneIndexInTree(
    const HdSceneIndexBaseRefPtr&                             sceneIndex,
    const std::function<bool(const HdSceneIndexBaseRefPtr&)>& predicate
);

/**
* @class A utility class to accumulate and read SceneIndex notifications sent by a SceneIndex.
*/
class SceneIndexNotificationsAccumulator : public HdSceneIndexObserver
{
public:
    SceneIndexNotificationsAccumulator(HdSceneIndexBaseRefPtr observedSceneIndex)
        : _observedSceneIndex(observedSceneIndex)
    {
        _observedSceneIndex->AddObserver(HdSceneIndexObserverPtr(this));
    }
    ~SceneIndexNotificationsAccumulator() override
    {
        _observedSceneIndex->RemoveObserver(HdSceneIndexObserverPtr(this));
    }

    HdSceneIndexBaseRefPtr GetObservedSceneIndex() { return _observedSceneIndex; }

    const AddedPrimEntries&   GetAddedPrimEntries() { return _addedPrimEntries; }
    const RemovedPrimEntries& GetRemovedPrimEntries() { return _removedPrimEntries; }
    const DirtiedPrimEntries& GetDirtiedPrimEntries() { return _dirtiedPrimEntries; }
    const RenamedPrimEntries& GetRenamedPrimEntries() { return _renamedPrimEntries; }

    void PrimsAdded(const HdSceneIndexBase& sender, const AddedPrimEntries& entries) override
    {
        _addedPrimEntries.insert(_addedPrimEntries.end(), entries.begin(), entries.end());
    }

    void PrimsRemoved(const HdSceneIndexBase& sender, const RemovedPrimEntries& entries) override
    {
        _removedPrimEntries.insert(_removedPrimEntries.end(), entries.begin(), entries.end());
    }

    void PrimsDirtied(const HdSceneIndexBase& sender, const DirtiedPrimEntries& entries) override
    {
        _dirtiedPrimEntries.insert(_dirtiedPrimEntries.end(), entries.begin(), entries.end());
    }

    void PrimsRenamed(const HdSceneIndexBase& sender, const RenamedPrimEntries& entries) override
    {
        _renamedPrimEntries.insert(_renamedPrimEntries.end(), entries.begin(), entries.end());
    }

private:
    HdSceneIndexBaseRefPtr _observedSceneIndex;

    AddedPrimEntries   _addedPrimEntries;
    DirtiedPrimEntries _dirtiedPrimEntries;
    RemovedPrimEntries _removedPrimEntries;
    RenamedPrimEntries _renamedPrimEntries;
};

PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

/**
 * @brief Set global command-line arguments for use in tests.
 *
 * Provide argc, argv access for GoogleTest unit tests.
 *
 * @param[in] argc The count of arguments in argv.
 * @param[in] argv Argument character strings.
 */
void setTestingArgs(int argc, char** argv);

/**
 * @brief Get global command-line arguments for use in tests.
 *
 * Provide argc, argv access for GoogleTest unit tests.
 *
 * @return argc, argv.
 */
std::pair<int, char**> getTestingArgs();

}

#endif // MAYAHYDRA_TEST_UTILS_H
