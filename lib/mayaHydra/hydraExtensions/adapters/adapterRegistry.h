//
// Copyright 2019 Luma Pictures
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
#ifndef MAYAHYDRALIB_ADAPTER_REGISTRY_H
#define MAYAHYDRALIB_ADAPTER_REGISTRY_H

#include <mayaHydraLib/adapters/cameraAdapter.h>
#include <mayaHydraLib/adapters/lightAdapter.h>
#include <mayaHydraLib/adapters/materialAdapter.h>
#include <mayaHydraLib/adapters/renderItemAdapter.h>
#include <mayaHydraLib/adapters/shapeAdapter.h>

#include <pxr/base/tf/singleton.h>
#include <pxr/pxr.h>

#include <maya/MFn.h>

#include <unordered_map>

PXR_NAMESPACE_OPEN_SCOPE

/**
 * \brief An adapter is used to translate from Maya data to hydra data. MayaHydraAdapterRegistry is
 * used to register/retrieve the adapters.
 */

class MayaHydraAdapterRegistry : public TfSingleton<MayaHydraAdapterRegistry>
{
    friend class TfSingleton<MayaHydraAdapterRegistry>;
    MAYAHYDRALIB_API
    MayaHydraAdapterRegistry() = default;

public:
    using ShapeAdapterCreator
        = std::function<MayaHydraShapeAdapterPtr(MayaHydraSceneIndex*, const MDagPath&)>;
    MAYAHYDRALIB_API
    static void RegisterShapeAdapter(const TfToken& type, ShapeAdapterCreator creator);

    MAYAHYDRALIB_API
    static ShapeAdapterCreator GetShapeAdapterCreator(const MDagPath& dag);

    using LightAdapterCreator
        = std::function<MayaHydraLightAdapterPtr(MayaHydraSceneIndex*, const MDagPath&)>;
    MAYAHYDRALIB_API
    static void RegisterLightAdapter(const TfToken& type, LightAdapterCreator creator);

    MAYAHYDRALIB_API
    static LightAdapterCreator GetLightAdapterCreator(const MDagPath& dag);

    MAYAHYDRALIB_API
    static LightAdapterCreator GetLightAdapterCreator(const MObject& dag);

    using MaterialAdapterCreator = std::function<
        MayaHydraMaterialAdapterPtr(const SdfPath&, MayaHydraSceneIndex*, const MObject&)>;
    MAYAHYDRALIB_API
    static void RegisterMaterialAdapter(const TfToken& type, MaterialAdapterCreator creator);

    MAYAHYDRALIB_API
    static MaterialAdapterCreator GetMaterialAdapterCreator(const MObject& node);

    using CameraAdapterCreator
        = std::function<MayaHydraCameraAdapterPtr(MayaHydraSceneIndex*, const MDagPath&)>;
    MAYAHYDRALIB_API
    static void RegisterCameraAdapter(const TfToken& type, CameraAdapterCreator creator);

    MAYAHYDRALIB_API
    static CameraAdapterCreator GetCameraAdapterCreator(const MObject& node);

    MAYAHYDRALIB_API
    static CameraAdapterCreator GetCameraAdapterCreator(const MDagPath& dag);

    /// Register the adapters of the libraries that are loaded, and load the
    /// libraries of the adapters declared in a plugInfo.json.
    ///
    /// The adapters of this library need no plugInfo.json: the library is
    /// loaded, so their TF_REGISTRY_FUNCTIONs run from here.
    ///
    /// An adapter in another library needs one, unless something else loads
    /// that library (a Maya plug-in the user loads, say). It declares the
    /// adapter's type with one of the adapter types as base, and names the
    /// library:
    ///
    ///     "Types": { "MyAdapter": { "bases": ["MayaHydraShapeAdapter"] } }
    ///
    /// That is how the library is found and loaded here. Without it, nothing
    /// knows of the library: its adapter is never registered and its nodes are
    /// not translated, without any error. See the example in
    /// flowViewportAPIExamples/externalShapeAdapter.
    MAYAHYDRALIB_API
    static void LoadAllPlugin();

private:
    std::unordered_map<TfToken, ShapeAdapterCreator, TfToken::HashFunctor>    _dagAdapters;
    std::unordered_map<TfToken, LightAdapterCreator, TfToken::HashFunctor>    _lightAdapters;
    std::unordered_map<TfToken, MaterialAdapterCreator, TfToken::HashFunctor> _materialAdapters;
    std::unordered_map<TfToken, CameraAdapterCreator, TfToken::HashFunctor>   _cameraAdapters;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_ADAPTER_REGISTRY_H
