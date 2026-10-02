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

#include "mayaHydraPrimvarDataSource.h"

#include <mayaHydraLib/adapters/adapter.h>
#include <mayaHydraLib/adapters/renderItemAdapter.h>
#include <mayaHydraLib/adapters/shapeAdapter.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/imaging/hd/extGpuBufferSchema.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>

#include <cmath>
#include <limits>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

/// CPU fallback paired with an extGpuBuffer primvar. Constructing the
/// datasource does not touch the buffer; only GetValue() reads the shared GL
/// buffer and materializes a VtArray.
class _ExtGpuBufferLazyValueDataSource final
    : public HdSampledDataSource
{
public:
    HD_DECLARE_DATASOURCE(_ExtGpuBufferLazyValueDataSource);

    _ExtGpuBufferLazyValueDataSource(
        const TfToken&                  primvarName,
        MayaHydraRenderItemAdapter*     adapter)
        : _primvarName(primvarName)
        , _adapter(adapter)
    {
    }

    VtValue GetValue(Time shutterOffset) override
    {
        TF_UNUSED(shutterOffset);
        return _adapter
            ? _adapter->GetExtGpuBufferLazyValue(_primvarName)
            : VtValue();
    }

    bool GetContributingSampleTimesForInterval(
        Time startTime,
        Time endTime,
        std::vector<Time>* outSampleTimes) override
    {
        TF_UNUSED(startTime);
        TF_UNUSED(endTime);
        TF_UNUSED(outSampleTimes);
        return false;
    }

private:
    TfToken                       _primvarName;
    MayaHydraRenderItemAdapter*   _adapter;
};

} // namespace

MayaHydraPrimvarsDataSource::MayaHydraPrimvarsDataSource(
    MayaHydraAdapter* adapter)
    : _adapter(adapter)
{
}

void MayaHydraPrimvarsDataSource::AddDesc(
    const TfToken& name,
    const TfToken& interpolation,
    const TfToken& role, bool indexed)
{
    _entries[name] = { interpolation, role, indexed };
}

TfTokenVector MayaHydraPrimvarsDataSource::GetNames()
{
    TfTokenVector result;
    result.reserve(_entries.size());
    for (const auto& pair : _entries) {
        result.push_back(pair.first);
    }
    return result;
}

HdDataSourceBaseHandle MayaHydraPrimvarsDataSource::Get(const TfToken& name)
{
    _EntryMap::const_iterator it = _entries.find(name);
    if (it == _entries.end()) {
        return nullptr;
    }

    HdContainerDataSourceHandle extGpuBuffer;
    HdSampledDataSourceHandle value =
        MayaHydraPrimvarValueDataSource::New(name, _adapter);
    if (auto* ri = dynamic_cast<MayaHydraRenderItemAdapter*>(_adapter)) {
        extGpuBuffer = ri->GetExtGpuBufferSchema(name);
        if (extGpuBuffer) {
            value = _ExtGpuBufferLazyValueDataSource::New(name, ri);
        }
    }

    // Need to handle indexed case?
    assert(!(*it).second.indexed);
    HdContainerDataSourceHandle primvar = HdPrimvarSchema::Builder()
        .SetPrimvarValue(value)
        .SetInterpolation(HdPrimvarSchema::BuildInterpolationDataSource(
            (*it).second.interpolation))
        .SetRole(HdPrimvarSchema::BuildRoleDataSource(
            (*it).second.role))
        .Build();

    // When the adapter provides an externally-owned GPU buffer for this
    // primvar, overlay it as the `extGpuBuffer` child so a renderer can
    // consume it directly (HdExtGpuBufferSchema::GetFromParent).
    if (extGpuBuffer) {
        const TfToken names[1] = {
            HdExtGpuBufferSchemaTokens->extGpuBuffer };
        const HdDataSourceBaseHandle values[1] = { extGpuBuffer };
        primvar = HdOverlayContainerDataSource::New(
            HdRetainedContainerDataSource::New(1, names, values),
            primvar);
    }
    return primvar;
}

MayaHydraPrimvarValueDataSource::MayaHydraPrimvarValueDataSource(
    const TfToken& primvarName,
    MayaHydraAdapter* adapter)
    : _primvarName(primvarName)
    , _adapter(adapter)
{
}

void MayaHydraPrimvarValueDataSource::_EnsureSamples()
{
    if (_sampled) {
        return;
    }
    _sampled = true;

    // With motion blur off, GetValue and GetContributingSampleTimesForInterval fall
    // back to the single live value, so no SamplePrimvar work is done.
    MayaHydraSceneIndex* sceneIndex = _adapter ? _adapter->GetMayaHydraSceneIndex() : nullptr;
    if (!sceneIndex || !sceneIndex->GetParams().motionSamplesEnabled()) {
        return;
    }

    // Only shape adapters can multi-sample primvars, e.g. deforming mesh points.
    MayaHydraShapeAdapter* shapeAdapter = dynamic_cast<MayaHydraShapeAdapter*>(_adapter);
    if (!shapeAdapter) {
        return;
    }

    float   times[kMotionKeys] {};
    VtValue values[kMotionKeys];
    const size_t count = shapeAdapter->SamplePrimvar(_primvarName, kMotionKeys, times, values);
    if (count == 0) {
        return;
    }

    _samples.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        _samples.emplace_back(times[i], std::move(values[i]));
    }
}

VtValue MayaHydraPrimvarValueDataSource::GetValue(Time shutterOffset)
{
    _EnsureSamples();
    if (_samples.size() <= 1) {
        return _adapter->Get(_primvarName);
    }
    // Consumers query at exactly the contributing times reported below.
    size_t best = 0;
    float  bestDist = std::numeric_limits<float>::max();
    for (size_t i = 0; i < _samples.size(); ++i) {
        const float d = std::abs(_samples[i].first - static_cast<float>(shutterOffset));
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return _samples[best].second;
}

bool MayaHydraPrimvarValueDataSource::GetContributingSampleTimesForInterval(
    Time startTime, Time endTime,
    std::vector<Time>* outSampleTimes)
{
    _EnsureSamples();
    if (_samples.size() <= 1) {
        return false;
    }
    if (outSampleTimes) {
        outSampleTimes->clear();
        outSampleTimes->reserve(_samples.size());
        for (const auto& sample : _samples) {
            outSampleTimes->push_back(sample.first);
        }
    }
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
