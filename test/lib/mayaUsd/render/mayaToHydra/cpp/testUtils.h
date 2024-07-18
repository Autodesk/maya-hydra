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

#include <flowViewport/sceneIndex/fvpSelectionSceneIndex.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/visibilitySchema.h>

#include <maya/MStatus.h>
#include <maya/MApiNamespace.h>

#include <QMouseEvent>
#include <QWidget>

#include <filesystem>
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
 * @brief Find the selection scene index in the scene index tree.
 *
 * This is a convenience function that calls findSceneIndexInTree() with
 * the appropriate predicate.
 *
 * @param[in] sceneIndex The root of the scene index tree to search.
 *
 * @return Selection scene index pointer if found, otherwise nullptr.
 */
Fvp::SelectionSceneIndexRefPtr findSelectionSceneIndexInTree(
    const HdSceneIndexBaseRefPtr& sceneIndex
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

/**
 * @brief Get the input directory used for test samples.
 *
 * @return Path to the directory containing test samples.
 */
std::filesystem::path getInputDir();

/**
 * @brief Set the input directory used for test samples.
 *
 * @param[in] inputDir Path to the directory containing test samples.
 */
void setInputDir(std::filesystem::path inputDir);

/**
 * @brief Get the output directory used for test output files.
 *
 * @return Path to the test output files directory.
 */
std::filesystem::path getOutputDir();

/**
 * @brief Set the output directory used for test output files.
 *
 * @param[in] outputDir Path to the test output files directory.
 */
void setOutputDir(std::filesystem::path outputDir);

/**
 * @brief Get the full path to a test sample file.
 *
 * @param[in] filename Name of the sample file (including its extension, if any).
 *
 * @return Full path to the sample file.
 */
std::filesystem::path getPathToSample(std::string filename);

/**
 * @brief Compares a data source text dump to a reference dump. The text dump will be also be
 * written to a file in the output directory.
 *
 * @param[in] dataSource The data source to dump and compare to a reference.
 * @param[in] referencePath The path to the reference dump file.
 *
 * @return Whether the data source dump matches the reference dump.
 */
bool dataSourceMatchesReference(
    PXR_NS::HdDataSourceBaseHandle dataSource,
    std::filesystem::path          referencePath);

#ifdef CONFIGURABLE_DECIMAL_STREAMING_AVAILABLE
/**
* @class A RAII-style class to temporarily override the string conversion settings used when
* streaming out VtValues containing floats or doubles.
*/
class DecimalStreamingOverride {
public:
    DecimalStreamingOverride(const pxr::TfDecimalToStringConfig& overrideConfig)
    {
        _prevFloatConfig = pxr::TfStreamFloat::ToStringConfig();
        _prevDoubleConfig = pxr::TfStreamDouble::ToStringConfig();
        pxr::TfStreamFloat::ToStringConfig() = overrideConfig;
        pxr::TfStreamDouble::ToStringConfig() = overrideConfig;
    }
    ~DecimalStreamingOverride()
    {
        pxr::TfStreamFloat::ToStringConfig() = _prevFloatConfig;
        pxr::TfStreamDouble::ToStringConfig() = _prevDoubleConfig;
    }
private:
    pxr::TfDecimalToStringConfig _prevFloatConfig;
    pxr::TfDecimalToStringConfig _prevDoubleConfig;
};
#endif

/**
 * @brief Send a mouse move event to a widget to move the mouse at a given position.
 *
 * @param[in] widget The widget to send the event to.
 * @param[in] localMousePos The position to move the mouse to, relative to the widget.
 *
 */
void mouseMoveTo(QWidget* widget, QPoint localMousePos);

/**
 * @brief Send a mouse press event to a widget to press a mouse button at a given position.
 *
 * @param[in] mouseButton The mouse button to press.
 * @param[in] widget The widget to send the event to.
 * @param[in] localMousePos The position of the mouse, relative to the widget.
 *
 */
void mousePress(Qt::MouseButton mouseButton, QWidget* widget, QPoint localMousePos);

/**
 * @brief Send a mouse release event to a widget to release a mouse button at a given position.
 *
 * @param[in] mouseButton The mouse button to release.
 * @param[in] widget The widget to send the event to.
 * @param[in] localMousePos The position of the mouse, relative to the widget.
 *
 */
void mouseRelease(Qt::MouseButton mouseButton, QWidget* widget, QPoint localMousePos);

/**
 * @brief Convenience function to send a mouse press / release event pair to a widget at a given position.
 *
 * @param[in] mouseButton The mouse button to press and release.
 * @param[in] widget The widget to send the event to.
 * @param[in] localMousePos The position of the mouse, relative to the widget.
 *
 */
void mouseClick(Qt::MouseButton mouseButton, QWidget* widget, QPoint localMousePos);

/**
 * @brief Get the mouse coordinates for a scene index prim.
 *
 * This function will return the mouse coordinates for the scene index prim's
 * local coordinate origin.  Note that the view argument is not changed and is
 * passed in by non const reference only because its interface is not
 * const-correct.
 *
 * @param[in] prim The scene index prim for which mouse coordinates must be computed. 
 * @param[in] view The view for which mouse coordinates are returned.
 * @return Mouse coordinates.
 */
QPoint getPrimMouseCoords(const PXR_NS::HdSceneIndexPrim& prim, M3dView& view);

/**
 * @brief Ensures a selection highlight hierarchy is properly structured.
 *
 * This method takes in a path to a prim in a selection highlight hierarchy and ensures that 
 * the selection highlight graph is structured properly, and that the leaf mesh prims have
 * the proper display style.
 *
 * @param[in] sceneIndex The scene index containing the selection highlight hierarchy. 
 * @param[in] primPath The path to the base prim of the selection highlight hierarchy.
 * @param[in] selectionHighlightMirrorTag The tag marking what is a or is not a selection highlight prim.
 */
void assertSelectionHighlightCorrectness(
    const PXR_NS::HdSceneIndexBaseRefPtr& sceneIndex,
    const PXR_NS::SdfPath& primPath,
    const std::string& selectionHighlightMirrorTag,
    const PXR_NS::TfToken& leafDisplayStyle);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_TEST_UTILS_H
