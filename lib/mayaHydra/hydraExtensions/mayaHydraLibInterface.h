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

#ifndef MAYAHYDRALIB_INTERFACE_H
#define MAYAHYDRALIB_INTERFACE_H

//Local headers
#include "mayaHydraLib/api.h"

//Hydra headers
#include <pxr/imaging/hd/sceneIndex.h>


PXR_NAMESPACE_OPEN_SCOPE

using SceneIndicesVector                                        = std::vector<HdSceneIndexBasePtr>;//Be careful, these are not not Ref counted. Elements could become dangling

/// In order to access this interface, call the function GetMayaHydraLibInterface()
class MayaHydraLibInterface
{
public:
    /**
     * @brief Register a terminal scene index into the Hydra plugin.
     *
     * @param[in] sceneIndex is a pointer to the SceneIndex to be registered.
     */
    virtual void RegisterTerminalSceneIndex(const HdSceneIndexBaseRefPtr& sceneIndex) = 0;

    /**
     * @brief Unregister a terminal scene index from the Hydra plugin.
     *
     * @param[in] sceneIndex is a pointer to the SceneIndex to be unregistered.
     */
    virtual void UnregisterTerminalSceneIndex(const HdSceneIndexBaseRefPtr& sceneIndex) = 0;

    /**
     * @brief Clear the list of registered terminal scene indices
     *
     * This does not delete them, but just unregisters them.
     */
    virtual void ClearTerminalSceneIndices() = 0;

    /**
     * @brief Retrieve the list of registered terminal scene indices from the Hydra plugin.
     *
     * @return A const reference to the vector of registered terminal scene indices.
     */
    virtual const SceneIndicesVector& GetTerminalSceneIndices() const = 0;

    /**
    *  @brief      Callback function called when a scene index was removed by our Hydra viewport plugin
    *
    *  @param[in]  sceneIndex is the new HdSceneIndexBaseRefPtr being removed by our Hydra viewport plugin.
    */
    virtual void SceneIndexRemoved(const HdSceneIndexBaseRefPtr& sceneIndex) = 0;
};

/// Access the MayaHydraLibInterface instance
MAYAHYDRALIB_API
MayaHydraLibInterface& GetMayaHydraLibInterface();

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_INTERFACE_H
