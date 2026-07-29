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

#include <flowViewport/sceneIndex/fvpSceneIndexUtils.h>
#include <flowViewport/sceneIndex/fvpSelectionSceneIndex.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>

#include <maya/MStatus.h>
#include <maya/MApiNamespace.h>

#include <ufe/ufe.h>

#include <QMouseEvent>
#include <QWidget>

#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <string>
#include <string_view>

UFE_NS_DEF {
class Path;
}

PXR_NAMESPACE_OPEN_SCOPE

constexpr double DEFAULT_TOLERANCE = std::numeric_limits<double>::epsilon();

constexpr std::string_view kHighlightsHierarchyPrefix = "FlowViewportSelectionHighlights";

using SceneIndicesVector = std::vector<HdSceneIndexBaseRefPtr>;

/**
 * @brief Retrieve the list of registered terminal scene indices from the Hydra plugin
 *
 * @return A reference to the vector of registered terminal scene indices.
 */
const SceneIndicesVector& GetTerminalSceneIndices();

/**
 * @brief Retrieve the scene index associated to a given pass
 *
 * @return A reference to the scene index of the desired pass.
 */
const HdSceneIndexBasePtr GetPassSceneIndex(int passIndex);

/**
 * @brief Retrieve the scene index associated to the beauty pass
 *
 * @return A reference to the beauty pass scene index.
 */
const HdSceneIndexBasePtr GetBeautyPassSceneIndex();

/**
 * @brief Retrieve the scene index associated to the secondary graphics pass
 *
 * @return A reference to the secondary graphics pass scene index.
 */
const HdSceneIndexBasePtr GetSecondaryGraphicsPassSceneIndex();

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

using Fvp::PrimEntry;
using Fvp::FindPrimPredicate;
using Fvp::PrimEntriesVector;
using Fvp::SceneIndexInspector;

/**
 * @brief Create a predicate that matches prims whose path contains primNamePart and whose type
 *        equals primType. Shared by mesh, camera, and light tests.
 *
 * @param[in] primNamePart Substring to match in the prim path (e.g. "pCube1", "perspShape").
 * @param[in] primType The expected prim type (e.g. HdPrimTypeTokens->mesh, HdPrimTypeTokens->camera).
 *
 * @return A FindPrimPredicate that can be passed to SceneIndexInspector::FindPrims.
 */
FindPrimPredicate CreatePrimPredicate(
    const std::string&     primNamePart,
    const PXR_NS::TfToken& primType);

/**
 * @brief Find the first terminal scene index containing a prim that matches the predicate.
 *
 * @param[in] sceneIndices Terminal scene indices to search.
 * @param[in] predicate Predicate used to match a prim.
 * @param[in] maxPrims Maximum prims to fetch per scene index (default 1).
 *
 * @return The first matching terminal scene index, or nullptr if none match.
 */
HdSceneIndexBaseRefPtr FindTerminalSceneIndexWithPrim(
    const SceneIndicesVector& sceneIndices,
    FindPrimPredicate         predicate,
    size_t                    maxPrims = 1);

/**
 * @brief Find the first terminal scene index containing a prim with name substring and type.
 *
 * @param[in] sceneIndices Terminal scene indices to search.
 * @param[in] primNamePart Substring to match in the prim path.
 * @param[in] primType Expected prim type token.
 * @param[in] maxPrims Maximum prims to fetch per scene index (default 1).
 *
 * @return The first matching terminal scene index, or nullptr if none match.
 */
HdSceneIndexBaseRefPtr FindTerminalSceneIndexWithPrim(
    const SceneIndicesVector& sceneIndices,
    const std::string&        primNamePart,
    const PXR_NS::TfToken&    primType,
    size_t                    maxPrims = 1);

/**
 * @brief Retrieve an optionVar string value or return a fallback.
 *
 * @param[in] optionVar The optionVar name to read.
 * @param[in] fallback The fallback string if the optionVar does not exist.
 *
 * @return The optionVar value as a std::string, or the fallback.
 */
std::string GetOptionVarOrDefault(const char* optionVar, const char* fallback);

/**
 * @brief Extract the leaf shape name from a Maya DAG path.
 *
 * @param[in] fullPath Full DAG path (e.g. "|parent|shape").
 *
 * @return The leaf name (e.g. "shape"), or the full string if no pipe is found.
 */
std::string GetShapeNameFromFullPath(const std::string& fullPath);

/**
 * @brief Extract the parent transform name from a Maya DAG path.
 *
 * @param[in] fullPath Full DAG path (e.g. "|parent|shape").
 *
 * @return The parent transform name (e.g. "parent"), or empty if none.
 */
std::string GetParentNameFromFullPath(const std::string& fullPath);

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

using Fvp::SceneIndexDisplayNamePred;
using Fvp::findSceneIndexInTree;

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
 * @brief Find the MayaHydraSceneIndex in the scene index tree.
 *
 * The MayaHydraSceneIndex is where Maya-to-Hydra adapters write prim data.
 * Use this when reading primvars to get the adapter output directly, bypassing
 * any filtering in downstream scene indices (e.g. delegate-specific terminals).
 *
 * @param[in] terminalSceneIndex A terminal scene index from GetTerminalSceneIndices().
 *
 * @return MayaHydraSceneIndex pointer if found, otherwise nullptr.
 */
HdSceneIndexBaseRefPtr FindMayaHydraSceneIndex(
    const HdSceneIndexBaseRefPtr& terminalSceneIndex
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

    const AddedPrimEntries&   GetAddedPrimEntries() const { return _addedPrimEntries; }
    const RemovedPrimEntries& GetRemovedPrimEntries() const { return _removedPrimEntries; }
    const DirtiedPrimEntries& GetDirtiedPrimEntries() const { return _dirtiedPrimEntries; }
    const RenamedPrimEntries& GetRenamedPrimEntries() const { return _renamedPrimEntries; }

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

/// Summary of mesh-relevant dirty locators for one prim since a start index.
/// Used by tests to verify topology vs deformation emission; see also
/// doc/render_delegate_topology_vs_deformation.md
struct MeshDirtySignals
{
    bool anyForPrim{false};
    bool meshTopology{false};
    bool broadPrimvars{false};
    bool extCompPrimvars{false};
    bool points{false};
    bool extent{false};
    bool normals{false};
    bool uvs{false};              // primvars/st — granular UV locator
    bool tangents{false};         // primvars/tangents — granular tangents locator
    bool subdivisionTags{false};
    bool displayStyle{false};     // displayStyle — emitted on smooth mesh / refine level changes
    bool visibility{false};       // visibility schema — emitted on visibility / intermediateObject changes
    bool instancer{false};        // instancedBy / instancerTopology — instance visibility or transform changes
};

/// Classify mesh dirty locators emitted for \p meshPrimPath since \p startIndex.
MeshDirtySignals ClassifyMeshDirtySince(
    const SceneIndexNotificationsAccumulator& accumulator,
    size_t                                    startIndex,
    const SdfPath&                            meshPrimPath);

/// Human-readable dump of dirtied entries since \p startIndex. When \p primPath is
/// non-empty, only entries for that prim are included.
std::string DescribeDirtyPrimEntriesSince(
    const SceneIndexNotificationsAccumulator& accumulator,
    size_t                                    startIndex,
    const SdfPath&                            primPath = SdfPath());

/// Resolve a mesh rprim path and MayaHydraSceneIndex from a Maya shape DAG path.
bool TryFindMeshPrim(
    const std::string&      meshShapeFull,
    SdfPath*                outPrimPath,
    HdSceneIndexBaseRefPtr* outMayaSceneIndex);

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

/**
 * @brief Returns the path where dataSourceMatchesReference writes the actual output.
 * Use in failure messages to help developers locate and diff the actual output.
 */
std::filesystem::path getDataSourceComparisonOutputPath(std::filesystem::path referencePath);

#ifdef CONFIGURABLE_DECIMAL_STREAMING_AVAILABLE
/**
* @class A RAII-style class to temporarily override the string conversion settings used when
* streaming out VtValues containing floats or doubles.
*/
class DecimalStreamingOverride {
public:
    DecimalStreamingOverride(const PXR_NS::TfDecimalToStringConfig& overrideConfig)
    {
        _prevFloatConfig = PXR_NS::TfStreamFloat::ToStringConfig();
        _prevDoubleConfig = PXR_NS::TfStreamDouble::ToStringConfig();
        PXR_NS::TfStreamFloat::ToStringConfig() = overrideConfig;
        PXR_NS::TfStreamDouble::ToStringConfig() = overrideConfig;
    }
    ~DecimalStreamingOverride()
    {
        PXR_NS::TfStreamFloat::ToStringConfig() = _prevFloatConfig;
        PXR_NS::TfStreamDouble::ToStringConfig() = _prevDoubleConfig;
    }
private:
    PXR_NS::TfDecimalToStringConfig _prevFloatConfig;
    PXR_NS::TfDecimalToStringConfig _prevDoubleConfig;
};
#endif

/**
 * @brief Predicate to return if global command-line arguments are empty.
 *
 */
bool testingArgsEmpty();

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
 * @brief Return whether the prim is visible or not.
 */
bool visibility(const PXR_NS::HdSceneIndexBasePtr& sceneIndex, const PXR_NS::SdfPath& primPath);

/**
 * @brief Return whether argument path vector contains the argument path.
 */
bool contains(const PXR_NS::SdfPathVector& paths, const PXR_NS::SdfPath& path);

/**
 * @brief Asserts that at least one prim matching the predicate carries a fully-selected
 * HdSelectionsSchema entry. Tolerates up to two matching prims.
 */
void ensureSelected(
    const PXR_NS::SceneIndexInspector& inspector,
    const PXR_NS::FindPrimPredicate&   primPredicate);

/**
 * @brief Asserts that no prim matching the predicate has an HdSelectionsSchema defined.
 */
void ensureUnselected(
    const PXR_NS::SceneIndexInspector& inspector,
    const PXR_NS::FindPrimPredicate&   primPredicate);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_TEST_UTILS_H
