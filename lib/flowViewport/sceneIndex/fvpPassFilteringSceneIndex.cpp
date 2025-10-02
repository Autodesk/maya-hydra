// Copyright 2025 Autodesk
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

#ifdef VIEWPORT_TOOLBOX

#include "fvpPassFilteringSceneIndex.h"
#include "flowViewport/fvpUtils.h"

#include <pxr/imaging/hd/purposeSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>


// For debugging purpose, set USE_LOGGING_VERSION to 1 to use logging version in _IsFilteredOut function, 0 for no logging version
// This will create a text file with the list of all prims tested for filtering and the 
// result and reason why it was filtered in/out.
// Modify "logFilename" later in the code to match your configuration.
#define USE_LOGGING_VERSION 0

#if USE_LOGGING_VERSION
    #include <fstream>
#endif

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// Check if the given SdfPath is prefixed by any of the paths in the vector.
bool isPrefixedBySdfPath(const SdfPath& pathToCheck, const SdfPathVector& paths)
{
    if (paths.empty()) {
        return false;
    }

    for (const auto& path : paths) {
        if (pathToCheck.HasPrefix(path)) {
            return true;
        }
    }

    return false;
}

#if USE_LOGGING_VERSION
    // Write to file periodically (every 10 calls to avoid too much I/O)
    static int callCount = 0;
#endif
} // namespace

namespace FVP_NS_DEF {
    
PassFilteringSceneIndexRefPtr PassFilteringSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const Fvp::FramePassConstDataPtr& framePassData)
{
    // If we return TfCreateRefPtr directly, Clang will destroy the rvalue before
    // returning, which means we will return a null pointer. To avoid this, store 
    // the pointer in an lvalue first and return that.
    auto refPtr = TfCreateRefPtr(new PassFilteringSceneIndex(inputSceneIndex, framePassData));
    return refPtr;
}

PassFilteringSceneIndex::PassFilteringSceneIndex(
    HdSceneIndexBaseRefPtr const& inputSceneIndex,
    const Fvp::FramePassConstDataPtr& framePassData)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
    , _framePassData(framePassData)
{
}

bool PassFilteringSceneIndex::_ShouldIncludeInAllPasses(const HdSceneIndexPrim& prim) const
{
    if (_framePassData && _framePassData->_keepLights && HdPrimTypeIsLight(prim.primType)) {
        return true;
    }

    return (!HdPrimTypeIsGprim(prim.primType)); // Non geometric prims are included in all passes
}

#if USE_LOGGING_VERSION
    // Keep the global static variables for logging
    static std::map<std::string, std::vector<std::tuple<std::string, bool, std::string>>> passResults;
    static std::mutex                                                       logMutex;
    static bool                                                             firstCall = true;
    static const std::string logFilename = "pass_filtering_log.txt";

void PassFilteringSceneIndex::ResetPassFilteringLog()
{
    std::lock_guard<std::mutex> lock(logMutex);
    passResults.clear();

    // Clear the log file
    std::ofstream logFile(logFilename, std::ios::trunc);
    logFile.close();

    // Reset the first call flag so logging will be enabled again
    firstCall = true;
}

    // Single function with conditional compilation
bool PassFilteringSceneIndex::_IsFilteredOut(const SdfPath& primPath) const
{

   // Logging version with full debug output
    if (firstCall) {
        // Clear the log file at the start
        std::ofstream logFile(logFilename, std::ios::trunc);
        logFile.close();
        firstCall = false;
        callCount = 0;
    }

    // Get pass name for logging - use frame pass name
    std::string passName = _framePassData && _framePassData->_framePass
        ? _framePassData->_framePass->GetName()
        : "Unknown";
    if (passName == "Unknown") {
        return true; // Safety check, exclude if no pass name
    }

    bool        result = false; // Will store the final result
    std::string reason = "";    // For debugging

    if (primPath.IsAbsoluteRootPath()) {
        result = false; // Include the root prim
        reason = "Is the root prim";
    } else
    if (!_framePassData) {
        result = false; // No filtering data, include the prim
        reason = "No filtering data";
    } else {
        // Fast checks that don't require GetPrim() - do these first for performance
        // Include paths: if specified and prim matches, definitely include it (workaround for special cases)
        if (!_framePassData->_includePaths.empty()
            && isPrefixedBySdfPath(primPath, _framePassData->_includePaths)) {
            result = false; // Prim is in the include paths, it will be rendered
            reason = "Included by path";
        }
        // Exclude paths: if specified and prim matches, definitely exclude it (workaround for special cases)
        else if (!_framePassData->_excludePaths.empty()
            && isPrefixedBySdfPath(primPath, _framePassData->_excludePaths)) {
            result = true; // Prim is in the exclude paths, it will be skipped
            reason = "Excluded by path";
        }
        
        // If we have a result from the fast checks, return it immediately
        if (reason == "Included by path" || reason == "Excluded by path") {
            return result;
        } else {
            // Now do the expensive GetPrim() call only when necessary
            const HdSceneIndexBaseRefPtr& inputSceneIndex = GetInputSceneIndex();
            if (!inputSceneIndex) {
                result = false; // No input scene index, include by default
                reason = "No input scene index";
            } else {
                HdSceneIndexPrim prim = inputSceneIndex->GetPrim(primPath);
                
                // Check if this prim should be included in all passes (non-geometric prims)
                if (prim.dataSource && _ShouldIncludeInAllPasses(prim)) {
                    result = false; // Include non-geometric prims in all passes
                    reason = "Non-geometric prim (included in all passes)";
                } else {
                    // Now apply the main filtering logic based on purpose render tags
                    if (prim.dataSource) {
                        const bool passSupportsPrimsWithNoPurposeRenderTag
                            = _framePassData->_supportPrimsWithNoPurposeRenderTag;

                        const TfToken purposeRenderTag = GetPurposeRenderTag(prim.dataSource);//From fvpUtils
                        if (!purposeRenderTag.IsEmpty()) {
                            result = (_framePassData->_includeRenderTags.find(purposeRenderTag)
                                     == _framePassData->_includeRenderTags.end());
                            reason = result ? "Purpose tag not in include list"
                                            : "Purpose tag matched";
                        } else {
                            // Geometric prim without purpose tag
                            if (passSupportsPrimsWithNoPurposeRenderTag) {
                                result = false;
                                reason = "Geometric prim without purpose tag but supported";
                            } else {
                                result = true; // No purpose render tag and not supported, so skip it
                                reason = "Geometric prim without purpose tag and not supported";
                            }
                        }
                    } else {
                        result = false; // Default to include
                        reason = "Geometric prim without data source";
                    }
                }
            }
        }
    }

    // Log the result for render tag filtering cases
    // Log the result
    {
        std::lock_guard<std::mutex> lock(logMutex);
        passResults[passName].emplace_back(primPath.GetText(), result, reason);
    }

    // Write to file periodically (every 10 calls to avoid too much I/O)
    if (++callCount % 10 == 0) {
        std::lock_guard<std::mutex> lock(logMutex);
        std::ofstream               logFile(logFilename, std::ios::trunc);

        // Create a sorted copy of the pass entries
        std::vector<std::pair<std::string, std::vector<std::tuple<std::string, bool, std::string>>>>
            sortedPasses;
        for (const auto& passEntry : passResults) {
            sortedPasses.emplace_back(passEntry.first, passEntry.second);
        }

        // Sort by pass name
        std::sort(
            sortedPasses.begin(),
            sortedPasses.end(),
            [](const std::pair<std::string, std::vector<std::tuple<std::string, bool, std::string>>>& a,
                const std::pair<std::string, std::vector<std::tuple<std::string, bool, std::string>>>& b) {
                return a.first < b.first; // Sort by pass name
            });

        for (const auto& passEntry : sortedPasses) {
            logFile << "/" << passEntry.first << "\n";

            // Create a sorted copy of the prim entries for this pass
            std::vector<std::tuple<std::string, bool, std::string>> sortedPrims = passEntry.second;
            std::sort(
                sortedPrims.begin(),
                sortedPrims.end(),
                [](const std::tuple<std::string, bool, std::string>& a,
                    const std::tuple<std::string, bool, std::string>& b) {
                    return std::get<0>(a) < std::get<0>(b); // Sort by prim path (first element)
                });

            for (const auto& primEntry : sortedPrims) {
                logFile << "   " << std::get<0>(primEntry) << " - "
                        << (std::get<1>(primEntry) ? "FILTERED_OUT" : "INCLUDED") 
                        << " (" << std::get<2>(primEntry) << ")\n";
            }
            logFile << "\n";
        }
        logFile.close();
    }

    return result;
}

#else
//No logging version of these functions
void PassFilteringSceneIndex::ResetPassFilteringLog()
{
}

bool PassFilteringSceneIndex::_IsFilteredOut(const SdfPath& primPath) const
{
    // Clean version without logging
    if (!(_framePassData && _framePassData->_framePass) ) {
        return true; // Safety check, exclude if no pass name
    }

    if (primPath.IsAbsoluteRootPath()) {
        return false; // Always include root prims
    }

    // Fast checks that don't require GetPrim() - do these first for performance
    // Include paths: if specified and prim matches, definitely include it (workaround for special cases)
    if (!_framePassData->_includePaths.empty()
        && isPrefixedBySdfPath(primPath, _framePassData->_includePaths)) {
        return false; // Prim is in the include paths, it will be rendered
    }

    // Exclude paths: if specified and prim matches, definitely exclude it (workaround for special cases)
    if (!_framePassData->_excludePaths.empty()
        && isPrefixedBySdfPath(primPath, _framePassData->_excludePaths)) {
        return true; // Prim is in the exclude paths, it will be skipped
    }

    // Now do the expensive GetPrim() call only when necessary
    const HdSceneIndexBaseRefPtr& inputSceneIndex = GetInputSceneIndex();
    if (!inputSceneIndex) {
        return false; // No input scene index, include by default
    }

    HdSceneIndexPrim prim = inputSceneIndex->GetPrim(primPath);
    
    // Check if this prim should be included in all passes (non-geometric prims)
    if (prim.dataSource && _ShouldIncludeInAllPasses(prim)) {
        return false; // Include non-geometric prims in all passes
    }

    // Now apply the main filtering logic based on purpose render tags
    if (prim.dataSource) {
        const bool passSupportsPrimsWithNoPurposeRenderTag
            = _framePassData->_supportPrimsWithNoPurposeRenderTag;

        const TfToken purposeRenderTag = GetPurposeRenderTag(prim.dataSource);//From fvpUtils
        if (!purposeRenderTag.IsEmpty()) {
            return (_framePassData->_includeRenderTags.find(purposeRenderTag)
                   == _framePassData->_includeRenderTags.end());
        } else {
            return !passSupportsPrimsWithNoPurposeRenderTag;
        }
    } else {
        // No data source
    }

    return false; // will be rendered.
}
#endif

HdSceneIndexPrim PassFilteringSceneIndex::GetPrim(const SdfPath& primPath) const
{
    if (_IsFilteredOut(primPath)) {
        return {}; // Return empty prim
    }
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector PassFilteringSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    // If this prim is filtered out, return no children
    if (_IsFilteredOut(primPath)) {
        return {};
    }

    // Get children from input scene index
    SdfPathVector childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);

    // Filter out children that should be filtered out
    SdfPathVector filteredChildPaths;
    for (const SdfPath& childPath : childPaths) {
        if (!_IsFilteredOut(childPath)) {
            filteredChildPaths.push_back(childPath);
        }
    }

    return filteredChildPaths;
}

void PassFilteringSceneIndex::DirtyPrimsFromPurposeRenderTag(const TfToken purposeRenderTag)
{
    HdSceneIndexObserver::AddedPrimEntries   addedEntries;
    auto& inputSceneIndex = GetInputSceneIndex();
    if (inputSceneIndex) { 
        for (const SdfPath& path : HdSceneIndexPrimView(inputSceneIndex)) {
            HdSceneIndexPrim prim = inputSceneIndex->GetPrim(path);
            const TfToken    purposeRenderTagFromPrim = GetPurposeRenderTag(prim.dataSource);//From fvpUtils
            if (purposeRenderTag == purposeRenderTagFromPrim){
                addedEntries.emplace_back(path, prim.primType);
            }
        }
    }
    if (!addedEntries.empty()) {
        _SendPrimsAdded(addedEntries);//Sending an add prim for an existing prim is equivalent to a resync
    }
}

} // namespace FVP_NS_DEF

#endif // VIEWPORT_TOOLBOX
