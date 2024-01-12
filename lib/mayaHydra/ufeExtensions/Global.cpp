//
// Copyright 2020 Autodesk
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
#include "Global.h"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <ufe/pathString.h>
#include <ufe/runTimeMgr.h>

#include <cctype>
#include <sstream>

namespace UfeExtensions {

PXR_NAMESPACE_USING_DIRECTIVE

constexpr char kMayaRunTimeName[] = "Maya-DG";
constexpr char kUSDRunTimeName[] = "USD";
constexpr char kSdfPathSeparator = '/';

Ufe::Rtid getMayaRunTimeId()
{
    static std::atomic<Ufe::Rtid> mayaRtid{0};
    if (!mayaRtid) {
        mayaRtid = Ufe::RunTimeMgr::instance().getId(kMayaRunTimeName);
    }
    return mayaRtid;
}

Ufe::Rtid getUsdRunTimeId()
{
    static std::atomic<Ufe::Rtid> usdRtid{0};
    if (!usdRtid) {
        usdRtid = Ufe::RunTimeMgr::instance().getId(kUSDRunTimeName);
    }
    return usdRtid;
}

MDagPath nameToDagPath(const std::string& name)
{
    MSelectionList selection;
    selection.add(MString(name.c_str()));
    MDagPath dag;
    // Not found?  Empty selection list.
    if (!selection.isEmpty()) {
        MStatus status = selection.getDagPath(0, dag);
        CHECK_MSTATUS(status);
    }
    return dag;
}

MDagPath ufeToDagPath(const Ufe::Path& ufePath)
{
    if (ufePath.runTimeId() != getMayaRunTimeId() || ufePath.nbSegments() > 1) {
        return MDagPath();
    }
    return nameToDagPath(Ufe::PathString::string(ufePath));
}

//! \brief Converts a SdfPath to Ufe PathSegment
///
/// In order to ensure compatibility with an arbitrary data model, it is possible to provide the
/// desired runtime id as a parameter.
///
//! \return Ufe PathSegment
Ufe::PathSegment sdfPathToUfePathSegment(const SdfPath& usdPath, Ufe::Rtid rtid, int instanceIndex/*= ALL_INSTANCES*/)
{
    static const char separator = SdfPathTokens->childDelimiter.GetText()[0u];

    if (!TF_VERIFY(!usdPath.IsEmpty())) {
        // Return an empty segment.
        return Ufe::PathSegment(Ufe::PathSegment::Components(), rtid, separator);
    }

    std::string pathString = usdPath.GetString();

    if (instanceIndex >= 0) {
        // Note here that we're taking advantage of the fact that identifiers
        // in SdfPaths must be C/Python identifiers; that is, they must *not*
        // begin with a digit. This means that when we see a path component at
        // the end of a USD path segment that does begin with a digit, we can
        // be sure that it represents an instance index and not a prim or other
        // USD entity.
        pathString += TfStringPrintf("%c%d", separator, instanceIndex);
    }

    return Ufe::PathSegment(pathString, rtid, separator);
}

Ufe::PathSegment dagPathToUfePathSegment(const MDagPath& dagPath)
{
    MStatus status;
    // The Ufe path includes a prepended "world" that the dag path doesn't have
    size_t                       numUfeComponents = dagPath.length(&status) + 1;
    Ufe::PathSegment::Components components;
    components.resize(numUfeComponents);
    components[0] = Ufe::PathComponent("world");
    MDagPath path = dagPath; // make an editable copy

    Ufe::Rtid mayaRtid(getMayaRunTimeId());
    // Pop nodes off the path string one by one, adding them to the correct
    // position in the components vector as we go. Use i>0 as the stopping
    // condition because we've already written to element 0 of the components
    // vector.
    for (int i = numUfeComponents - 1; i > 0; i--) {
        MObject node = path.node(&status);

        if (MS::kSuccess != status)
            return Ufe::PathSegment("", mayaRtid, '|');

        std::string componentString(MFnDependencyNode(node).name(&status).asChar());

        if (MS::kSuccess != status)
            return Ufe::PathSegment("", mayaRtid, '|');

        components[i] = componentString;
        path.pop(1);
    }

    return Ufe::PathSegment(std::move(components), mayaRtid, '|');
}

} // namespace UfeExtensions
