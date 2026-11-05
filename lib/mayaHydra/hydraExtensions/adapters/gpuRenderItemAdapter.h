//
// Copyright 2026 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef MAYAHYDRALIB_GPU_RENDER_ITEM_ADAPTER_H
#define MAYAHYDRALIB_GPU_RENDER_ITEM_ADAPTER_H

#include <mayaHydraLib/adapters/mhExtGpuStreams.h>
#include <mayaHydraLib/adapters/renderItemAdapter.h>

#include <pxr/pxr.h>

#include <maya/MHWGeometry.h>

PXR_NAMESPACE_OPEN_SCOPE

class Hgi;

/**
 * \brief Render item adapter that shares the VP2 vertex buffers with the renderer as
 * HdExtGpuBufferSchema primvars instead of copying them to the CPU.
 *
 * Created in place of MayaHydraRenderItemAdapter for the render items accepted by IsEligible().
 * A stream that cannot be shared on a given update falls back to the base class CPU read.
 */
class MayaHydraGpuRenderItemAdapter : public MayaHydraRenderItemAdapter
{
public:
    MAYAHYDRALIB_API
    MayaHydraGpuRenderItemAdapter(
        const MDagPath&      dagPath,
        const SdfPath&       slowId,
        int                  fastId,
        MayaHydraSceneIndex* mayaHydraSceneIndex,
        const MRenderItem&   ri,
        TfToken              purposeRenderTag);

    /// Whether \p ri should be translated by this adapter: GPU buffer sharing is enabled
    /// (MAYAHYDRA_GPU_BUFFER_SHARING), the primitive maps to a mesh or basis curves, and the
    /// renderer's \p hgi can consume VP2's buffers.
    MAYAHYDRALIB_API
    static bool IsEligible(const MRenderItem& ri, Hgi* hgi);

    /// The streams this adapter shares with the renderer.
    const MhExtGpuStreams* GetExtGpuStreams() const override { return &_extStreams; }

protected:
    size_t    _StoredStreamCount(const TfToken& primvar) const override;
    void      _BeginGeometryUpdate(bool geomChanged, bool topoChanged) override;
    GfRange3d _ResolveBounds(const GfRange3d& renderItemBounds) const override;
    void      _ReadVertexStream(
        MVertexBuffer*         mvb,
        bool                   topoChanged,
        bool                   useMayaNormals,
        const StreamPresence& present,
        _StreamDirty&          dirty) override;
    void _EndGeometryUpdate(
        const StreamPresence& present,
        bool                   geomChanged,
        bool                   topoChanged,
        bool                   useMayaNormals,
        _StreamDirty&          dirty) override;
    bool _TopologyNeedsRebuild(bool emitTopologyLocators, bool hasCachedTopology) const override;

private:
    MhExtGpuStreams _extStreams;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_GPU_RENDER_ITEM_ADAPTER_H
