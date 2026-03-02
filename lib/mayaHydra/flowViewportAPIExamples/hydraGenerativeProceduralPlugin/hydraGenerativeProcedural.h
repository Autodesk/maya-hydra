//
// Copyright 2025 Autodesk, Inc. All rights reserved.
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

#ifndef MAYA_HYDRA_HYDRA_GENERATIVE_PROCEDURAL_H
#define MAYA_HYDRA_HYDRA_GENERATIVE_PROCEDURAL_H

#include <pxr/pxr.h>
#include <pxr/imaging/hdGp/generativeProceduralPlugin.h>
#include <pxr/imaging/hdGp/generativeProceduralPluginRegistry.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>

PXR_NAMESPACE_OPEN_SCOPE
/// Generates two cube mesh prims and injects them into the scene.
class HydraGenerativeProcedural final : public HdGpGenerativeProcedural
{
public:
    HydraGenerativeProcedural(const SdfPath &proceduralPrimPath);
    ~HydraGenerativeProcedural() override;

    DependencyMap UpdateDependencies(
        const HdSceneIndexBaseRefPtr &inputScene) override;

    ChildPrimTypeMap Update(
        const HdSceneIndexBaseRefPtr &inputScene,
        const ChildPrimTypeMap &previousResult,
        const DependencyMap &dirtiedDependencies,
        HdSceneIndexObserver::DirtiedPrimEntries *outputDirtiedPrims) override;

    HdSceneIndexPrim GetChildPrim(
        const HdSceneIndexBaseRefPtr &inputScene,
        const SdfPath &childPrimPath) override;

private:
    static const TfToken _cube0Name;   // "cube0"
    static const TfToken _cube1Name;   // "cube1"

    static const SdfPath _cube0Path;   // "cube0"
    static const SdfPath _cube1Path;   // "cube1"

    HdSceneIndexPrim _BuildCubePrim(
        float halfSize,
        const GfMatrix4d &transform,
        const SdfPath &materialPath) const;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
