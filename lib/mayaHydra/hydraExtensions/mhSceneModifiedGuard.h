//
// Copyright 2026 Autodesk, Inc. All rights reserved.
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

#ifndef MAYAHYDRALIB_SCENE_MODIFIED_GUARD_H
#define MAYAHYDRALIB_SCENE_MODIFIED_GUARD_H

#include <mayaHydraLib/api.h>

#include <maya/MGlobal.h>
#include <maya/MStatus.h>

namespace MAYAHYDRA_NS_DEF {

/**
 * @brief Keeps internal Maya-Hydra bookkeeping from marking the Maya scene as modified.
 *
 * Maya-Hydra adds attributes to defaultRenderGlobals, and makes MayaUSD populate the
 * UsdDefaultRenderDescription node, as a side effect of loading the plugin or of first using
 * Hydra. Neither is a user edit, but Maya flags the scene as modified, which makes the
 * "Scene Not Saved" dialog pop up when the user opens or creates a scene.
 *
 * The guard reads the modified state on construction. On destruction it clears the modified
 * state only if the scope turned it on, so a scene the user has really modified stays modified.
 *
 * Header-only, so that both the plugin and the Hydra extensions library can use it.
 */
class SceneModifiedGuard
{
public:
    SceneModifiedGuard()
        : _wasModified(_SceneModified())
    {
    }

    ~SceneModifiedGuard()
    {
        // Never throw from a destructor: if the query fails, leave the flag alone.
        bool modified = true;
        if (_QuerySceneModified(modified) && modified && !_wasModified) {
            MGlobal::executeCommand("file -modified 0");
        }
    }

    SceneModifiedGuard(const SceneModifiedGuard&) = delete;
    SceneModifiedGuard& operator=(const SceneModifiedGuard&) = delete;

private:
    // Returns false if the query failed.
    static bool _QuerySceneModified(bool& modified)
    {
        int           result = 0;
        const MStatus status = MGlobal::executeCommand("file -query -modified", result);
        if (status != MStatus::kSuccess) {
            return false;
        }
        modified = (result != 0);
        return true;
    }

    // A failed query counts as modified, so the guard never clears a flag it could not read.
    static bool _SceneModified()
    {
        bool modified = true;
        _QuerySceneModified(modified);
        return modified;
    }

    bool _wasModified { true };
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRALIB_SCENE_MODIFIED_GUARD_H
