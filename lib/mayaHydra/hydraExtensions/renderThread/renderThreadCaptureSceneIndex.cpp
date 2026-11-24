// Copyright 2026 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "renderThreadCaptureSceneIndex.h"

#include "renderThreadStats.h"

#include <mayaHydraLib/debugCodes.h>

// VtVisitValue dispatches every Vt value type, so all of them must be complete.
#include <pxr/base/arch/demangle.h>
#include <pxr/base/vt/typeHeaders.h>
#include <pxr/base/vt/visitValue.h>
#if __has_include(<pxr/base/vt/arrayEdit.h>)
#include <pxr/base/vt/arrayEdit.h>
#endif
#include <pxr/base/work/loops.h>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/categoriesSchema.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
// Some USD and TBB versions pull in windows.h, whose basetyps.h defines interface
// as struct; HdMaterialNetworkSchema has a member of that name.
#if defined(_WIN32) && defined(interface)
#undef interface
#endif
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/mergingSceneIndex.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/primOriginSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/purposeSchema.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/timeSampleArray.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/pathExpression.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_set>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// The shutter interval asked for when capturing multisampled values, in frames
// relative to the current one. Wider than any viewport shutter.
constexpr HdSampledDataSource::Time kSampleIntervalStart = -1.0f;
constexpr HdSampledDataSource::Time kSampleIntervalEnd = 1.0f;
constexpr HdSampledDataSource::Time kSampleTimeEpsilon = 1e-4f;

HdDataSourceBaseHandle _CopyDataSource(const HdDataSourceBaseHandle& ds);

// Retained copies typed the same as the original, so HdTypedSampledDataSource<T>::Cast
// still succeeds downstream. Null for a type Vt does not know.
struct _TypedCopy
{
    template <class T>
    HdSampledDataSourceHandle operator()(const T& value) const
    {
        return HdRetainedTypedSampledDataSource<T>::New(value);
    }

    HdSampledDataSourceHandle operator()(const VtValue& v) const
    {
        if (v.IsHolding<SdfPath>()) {
            return HdRetainedTypedSampledDataSource<SdfPath>::New(v.UncheckedGet<SdfPath>());
        }
        if (v.IsHolding<VtArray<SdfPath>>()) {
            return HdRetainedTypedSampledDataSource<VtArray<SdfPath>>::New(
                v.UncheckedGet<VtArray<SdfPath>>());
        }
        if (v.IsHolding<SdfPathVector>()) {
            return HdRetainedTypedSampledDataSource<SdfPathVector>::New(
                v.UncheckedGet<SdfPathVector>());
        }
        if (v.IsHolding<SdfAssetPath>()) {
            return HdRetainedTypedSampledDataSource<SdfAssetPath>::New(
                v.UncheckedGet<SdfAssetPath>());
        }
        if (v.IsHolding<VtArray<SdfAssetPath>>()) {
            return HdRetainedTypedSampledDataSource<VtArray<SdfAssetPath>>::New(
                v.UncheckedGet<VtArray<SdfAssetPath>>());
        }
        if (v.IsHolding<SdfPathExpression>()) {
            return HdRetainedTypedSampledDataSource<SdfPathExpression>::New(
                v.UncheckedGet<SdfPathExpression>());
        }
        if (v.IsHolding<HdDataSourceLocator>()) {
            return HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(
                v.UncheckedGet<HdDataSourceLocator>());
        }
        if (v.IsHolding<HdPrimOriginSchema::OriginPath>()) {
            return HdRetainedTypedSampledDataSource<HdPrimOriginSchema::OriginPath>::New(
                v.UncheckedGet<HdPrimOriginSchema::OriginPath>());
        }
        return nullptr;
    }
};

// A multisampled value captured over the whole capture interval, answered the
// way the live source would answer a narrower one. Unlike
// HdRetainedTypedMultisampledDataSource, it reports only the samples the
// requested interval needs, and it interpolates between samples.
template <class T>
class _CapturedSamples : public HdTypedSampledDataSource<T>
{
public:
    HD_DECLARE_DATASOURCE(_CapturedSamples<T>);

    _CapturedSamples(
        std::vector<HdSampledDataSource::Time> times,
        std::vector<VtValue>                   values,
        const T&                               frameValue)
        : _times(std::move(times))
        , _values(std::move(values))
        , _frameValue(frameValue)
    {
    }

    // The samples inside the interval, plus the nearest one beyond each edge
    // that has no sample of its own, as UsdImaging reports them. The frame
    // value is not a sample unless the source reported it as one.
    bool GetContributingSampleTimesForInterval(
        HdSampledDataSource::Time               startTime,
        HdSampledDataSource::Time               endTime,
        std::vector<HdSampledDataSource::Time>* outSampleTimes) override
    {
        const auto first = std::lower_bound(
            _times.begin(), _times.end(), startTime - kSampleTimeEpsilon);
        auto last = std::upper_bound(_times.begin(), _times.end(), endTime + kSampleTimeEpsilon);
        auto begin = first;
        if (begin != _times.begin()
            && (begin == _times.end() || std::fabs(*begin - startTime) >= kSampleTimeEpsilon)) {
            --begin;
        }
        if (last != _times.end()
            && (last == _times.begin() || std::fabs(*(last - 1) - endTime) >= kSampleTimeEpsilon)) {
            ++last;
        }
        if (std::distance(begin, last) < 2) {
            return false;
        }
        if (outSampleTimes) {
            outSampleTimes->assign(begin, last);
        }
        return true;
    }

    T GetTypedValue(HdSampledDataSource::Time shutterOffset) override
    {
        if (std::fabs(shutterOffset) < kSampleTimeEpsilon) {
            return _frameValue;
        }
        const auto next = std::lower_bound(_times.begin(), _times.end(), shutterOffset);
        if (next == _times.begin()) {
            return _Typed(_values.front());
        }
        if (next == _times.end()) {
            return _Typed(_values.back());
        }
        const size_t i = std::distance(_times.begin(), next);
        if (std::fabs(_times[i] - shutterOffset) < kSampleTimeEpsilon) {
            return _Typed(_values[i]);
        }
        const float alpha = (shutterOffset - _times[i - 1]) / (_times[i] - _times[i - 1]);
        return _Typed(HdResampleNeighbors(alpha, _values[i - 1], _values[i]));
    }

    VtValue GetValue(HdSampledDataSource::Time shutterOffset) override
    {
        return VtValue(GetTypedValue(shutterOffset));
    }

private:
    T _Typed(const VtValue& v) const
    {
        return v.IsHolding<T>() ? v.UncheckedGet<T>() : _frameValue;
    }

    const std::vector<HdSampledDataSource::Time> _times;
    const std::vector<VtValue>                   _values;
    const T                                      _frameValue;
};

struct _MultisampledCopy
{
    std::vector<HdSampledDataSource::Time>* times;
    std::vector<VtValue>*                   values;

    template <class T>
    HdSampledDataSourceHandle operator()(const T& frameValue) const
    {
        for (const VtValue& v : *values) {
            if (!v.IsHolding<T>()) {
                return nullptr;
            }
        }
        return _CapturedSamples<T>::New(std::move(*times), std::move(*values), frameValue);
    }

    HdSampledDataSourceHandle operator()(const VtValue&) const { return nullptr; }
};

HdDataSourceBaseHandle _CopySampled(const HdSampledDataSourceHandle& ds)
{
    if (dynamic_cast<MAYAHYDRA_NS_DEF::RenderThreadCaptureByReference*>(ds.get())) {
        return ds;
    }

    const VtValue                          value = ds->GetValue(0.0f);
    std::vector<HdSampledDataSource::Time> times;
    if (ds->GetContributingSampleTimesForInterval(kSampleIntervalStart, kSampleIntervalEnd, &times)
        && times.size() > 1) {
        // The value at offset 0 is the frame itself, which some sources answer
        // with the live value rather than a sample, so it is kept apart, exact.
        std::sort(times.begin(), times.end());
        std::vector<VtValue> values;
        values.reserve(times.size());
        for (const HdSampledDataSource::Time t : times) {
            values.push_back(std::fabs(t) < kSampleTimeEpsilon ? value : ds->GetValue(t));
        }
        if (HdSampledDataSourceHandle copy
            = VtVisitValue(value, _MultisampledCopy { &times, &values })) {
            return copy;
        }
    }

    if (value.IsEmpty()) {
        return HdRetainedSampledDataSource::New(value);
    }
    if (HdSampledDataSourceHandle copy = VtVisitValue(value, _TypedCopy {})) {
        return copy;
    }
    // A value type Vt does not know, such as HdTupleType or an external buffer
    // handle. These come from retained schema builders, so the original is kept.
    return ds;
}

HdContainerDataSourceHandle _CopyContainer(const HdContainerDataSourceHandle& ds)
{
    if (!ds) {
        return nullptr;
    }
    const TfTokenVector                 names = ds->GetNames();
    std::vector<HdDataSourceBaseHandle> values;
    values.reserve(names.size());
    for (const TfToken& name : names) {
        values.push_back(_CopyDataSource(ds->Get(name)));
    }
    return HdRetainedContainerDataSource::New(names.size(), names.data(), values.data());
}

HdDataSourceBaseHandle _CopyDataSource(const HdDataSourceBaseHandle& ds)
{
    if (!ds) {
        return nullptr;
    }
    if (HdContainerDataSourceHandle container = HdContainerDataSource::Cast(ds)) {
        return _CopyContainer(container);
    }
    if (HdVectorDataSourceHandle vector = HdVectorDataSource::Cast(ds)) {
        const size_t                        count = vector->GetNumElements();
        std::vector<HdDataSourceBaseHandle> values;
        values.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            values.push_back(_CopyDataSource(vector->GetElement(i)));
        }
        return HdRetainedSmallVectorDataSource::New(count, values.data());
    }
    if (HdSampledDataSourceHandle sampled = HdSampledDataSource::Cast(ds)) {
        return _CopySampled(sampled);
    }
    // Holds no data, such as HdBlockDataSource.
    return ds;
}

uint64_t _MicrosSince(std::chrono::steady_clock::time_point start)
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now() - start)
                                     .count());
}

// Re-copy only what \p dirty covers, reusing the rest of \p old.
HdContainerDataSourceHandle _Recopy(
    const HdContainerDataSourceHandle& old,
    const HdContainerDataSourceHandle& live,
    const HdDataSourceLocatorSet&      dirty)
{
    if (!live) {
        return nullptr;
    }
    if (!old || dirty.Contains(HdDataSourceLocator::EmptyLocator())) {
        return _CopyContainer(live);
    }

    // The names a clean locator covers are unchanged, so the old copy's names
    // plus the dirty ones are all there can be. Asking the live container is
    // slow at the end of a long chain of overlays.
    TfTokenVector names = old->GetNames();
    for (const HdDataSourceLocator& l : dirty) {
        const TfToken& first = l.GetFirstElement();
        if (std::find(names.begin(), names.end(), first) == names.end()) {
            names.push_back(first);
        }
    }

    TfTokenVector                       outNames;
    std::vector<HdDataSourceBaseHandle> values;
    outNames.reserve(names.size());
    values.reserve(names.size());
    for (const TfToken& name : names) {
        const HdDataSourceLocator locator(name);
        if (!dirty.Intersects(locator)) {
            outNames.push_back(name);
            values.push_back(old->Get(name));
            continue;
        }
        const HdDataSourceBaseHandle liveChild = live->Get(name);
        if (!liveChild) {
            continue;
        }
        outNames.push_back(name);
        if (dirty.Contains(locator)) {
            values.push_back(_CopyDataSource(liveChild));
            continue;
        }
        // Only part of this child changed: every dirty locator is below it.
        HdDataSourceLocatorSet below;
        for (const HdDataSourceLocator& l : dirty.Intersection(locator)) {
            below.insert(l.RemoveFirstElement());
        }
        const HdContainerDataSourceHandle oldContainer = HdContainerDataSource::Cast(old->Get(name));
        const HdContainerDataSourceHandle liveContainer = HdContainerDataSource::Cast(liveChild);
        values.push_back(
            oldContainer && liveContainer ? _Recopy(oldContainer, liveContainer, below)
                                          : _CopyDataSource(liveChild));
    }
    return HdRetainedContainerDataSource::New(outNames.size(), outNames.data(), values.data());
}

// A prim-level schema FindUnlistedNames probes for, with the names its container
// is expected to list, or null where they vary (primvars, material networks).
struct _ProbedSchema
{
    TfToken              name;
    const TfTokenVector* members;
};

const std::vector<_ProbedSchema>& _ProbedSchemas()
{
    static const std::vector<_ProbedSchema> schemas = {
        { HdXformSchema::GetSchemaToken(), &HdXformSchemaTokens->allTokens },
        { HdMeshSchema::GetSchemaToken(), &HdMeshSchemaTokens->allTokens },
        { HdBasisCurvesSchema::GetSchemaToken(), &HdBasisCurvesSchemaTokens->allTokens },
        { HdCameraSchema::GetSchemaToken(), &HdCameraSchemaTokens->allTokens },
        { HdLightSchema::GetSchemaToken(), &HdLightTokens->allTokens },
        { HdVisibilitySchema::GetSchemaToken(), &HdVisibilitySchemaTokens->allTokens },
        { HdPurposeSchema::GetSchemaToken(), &HdPurposeSchemaTokens->allTokens },
        { HdExtentSchema::GetSchemaToken(), &HdExtentSchemaTokens->allTokens },
        { HdInstancerTopologySchema::GetSchemaToken(),
          &HdInstancerTopologySchemaTokens->allTokens },
        { HdInstancedBySchema::GetSchemaToken(), &HdInstancedBySchemaTokens->allTokens },
        { HdLegacyDisplayStyleSchema::GetSchemaToken(),
          &HdLegacyDisplayStyleSchemaTokens->allTokens },
        { HdCategoriesSchema::GetSchemaToken(), nullptr },
        { HdPrimvarsSchema::GetSchemaToken(), nullptr },
        { HdMaterialSchema::GetSchemaToken(), nullptr },
        { HdMaterialBindingsSchema::GetSchemaToken(), nullptr },
    };
    return schemas;
}

// Whether \p ds gives a reader anything: a container listing something, a vector or a
// non-empty value. HdsiMaterialPrimvarTransferSceneIndex serves an empty primvars
// container for every prim, listing it only when a material is bound.
bool _Serves(const HdDataSourceBaseHandle& ds)
{
    if (!ds || std::dynamic_pointer_cast<HdBlockDataSource>(ds)) {
        return false;
    }
    if (HdSampledDataSourceHandle sampled = HdSampledDataSource::Cast(ds)) {
        return !sampled->GetValue(0.0f).IsEmpty();
    }
    if (HdContainerDataSourceHandle container = HdContainerDataSource::Cast(ds)) {
        return !container->GetNames().empty();
    }
    return true;
}

// UsdImaging's light container lists only a few of the params it serves; the others
// are there for GetLightParamValue, which Storm reads. The copy keeps them too.
HdContainerDataSourceHandle _CopyLight(const HdContainerDataSourceHandle& light)
{
    TfTokenVector names = light->GetNames();
    for (const TfToken& name : HdLightTokens->allTokens) {
        if (std::find(names.begin(), names.end(), name) == names.end()
            && _Serves(light->Get(name))) {
            names.push_back(name);
        }
    }
    std::vector<HdDataSourceBaseHandle> values;
    values.reserve(names.size());
    for (const TfToken& name : names) {
        values.push_back(_CopyDataSource(light->Get(name)));
    }
    return HdRetainedContainerDataSource::New(names.size(), names.data(), values.data());
}

// Copy of a light prim's \p live top level, its light container copied by _CopyLight
// and the rest taken from \p copy, or copied from \p live when \p copy is null.
HdContainerDataSourceHandle
_CopyLightPrim(const HdContainerDataSourceHandle& copy, const HdContainerDataSourceHandle& live)
{
    if (!live) {
        return copy;
    }
    const TfToken&                    lightName = HdLightSchema::GetSchemaToken();
    const HdContainerDataSourceHandle liveLight = HdContainerDataSource::Cast(live->Get(lightName));
    TfTokenVector                     names = copy ? copy->GetNames() : live->GetNames();
    if (liveLight && std::find(names.begin(), names.end(), lightName) == names.end()) {
        names.push_back(lightName);
    }
    std::vector<HdDataSourceBaseHandle> values;
    values.reserve(names.size());
    for (const TfToken& name : names) {
        if (name == lightName && liveLight) {
            values.push_back(_CopyLight(liveLight));
        } else {
            values.push_back(copy ? copy->Get(name) : _CopyDataSource(live->Get(name)));
        }
    }
    return HdRetainedContainerDataSource::New(names.size(), names.data(), values.data());
}

// \p dirty without what is below the light schema, which _CopyLightPrim recopies.
HdDataSourceLocatorSet _WithoutLight(const HdDataSourceLocatorSet& dirty)
{
    HdDataSourceLocatorSet rest;
    for (const HdDataSourceLocator& locator : dirty) {
        if (!locator.HasPrefix(HdLightSchema::GetDefaultLocator())) {
            rest.insert(locator);
        }
    }
    return rest;
}

void _FindUnlisted(
    const HdContainerDataSourceHandle& container,
    const TfTokenVector&               candidates,
    const HdDataSourceLocator&         prefix,
    std::vector<HdDataSourceLocator>&  out)
{
    // A retained container lists exactly what it holds.
    if (dynamic_cast<HdRetainedContainerDataSource*>(container.get())) {
        return;
    }
    const TfTokenVector names = container->GetNames();
    for (const TfToken& candidate : candidates) {
        if (std::find(names.begin(), names.end(), candidate) == names.end()
            && _Serves(container->Get(candidate))) {
            out.push_back(prefix.Append(candidate));
        }
    }
}

void _WarnUnlistedNames(const SdfPath& primPath, const HdSceneIndexPrim& prim)
{
    if (!TfDebug::IsEnabled(MAYAHYDRALIB_RENDER_THREAD_CAPTURE)) {
        return;
    }
    const std::vector<HdDataSourceLocator> unlisted
        = MAYAHYDRA_NS_DEF::RenderThreadCaptureSceneIndex::FindUnlistedNames(prim.dataSource);
    if (unlisted.empty()) {
        return;
    }
    // Once per prim type and name: the cause is a data source class, not a prim.
    static std::mutex                      warnedMutex;
    static std::unordered_set<std::string> warned;
    for (const HdDataSourceLocator& locator : unlisted) {
        if (locator.HasPrefix(HdLightSchema::GetDefaultLocator())) {
            continue; // Kept by _CopyLight.
        }
        const std::string key = prim.primType.GetString() + " " + locator.GetString();
        {
            std::lock_guard<std::mutex> lock(warnedMutex);
            if (!warned.insert(key).second) {
                continue;
            }
        }
        HdContainerDataSourceHandle owner = prim.dataSource;
        for (size_t i = 0; owner && i + 1 < locator.GetElementCount(); ++i) {
            owner = HdContainerDataSource::Cast(owner->Get(locator.GetElement(i)));
        }
        TF_WARN(
            "%s (%s) serves '%s' but its container (%s) does not list it in GetNames(), so "
            "the render thread's copy of the scene drops it.",
            primPath.GetText(),
            prim.primType.GetText(),
            locator.GetString().c_str(),
            owner ? ArchGetDemangled(typeid(*owner)).c_str() : "null");
    }
}

const HdDataSourceLocator& _SelectionsLocator()
{
    static const HdDataSourceLocator locator = HdSelectionsSchema::GetDefaultLocator();
    return locator;
}

// Whether \p locators name the selections or something in them. An ancestor,
// such as the whole prim, does not count: that recopies the prim instead.
bool _NamesSelections(const HdDataSourceLocatorSet& locators)
{
    return std::any_of(locators.begin(), locators.end(), [](const HdDataSourceLocator& l) {
        return l.HasPrefix(_SelectionsLocator());
    });
}

HdDataSourceLocatorSet _WithoutSelections(const HdDataSourceLocatorSet& locators)
{
    HdDataSourceLocatorSet result;
    for (const HdDataSourceLocator& l : locators) {
        if (!l.HasPrefix(_SelectionsLocator())) {
            result.insert(l);
        }
    }
    return result;
}

HdDataSourceBaseHandle _CopySelections(const HdContainerDataSourceHandle& live)
{
    return live ? _CopyDataSource(live->Get(HdSelectionsSchemaTokens->selections)) : nullptr;
}

} // namespace

namespace MAYAHYDRA_NS_DEF {

struct RenderThreadCaptureSceneIndex::_Selections
{
    HdDataSourceBaseHandle Get() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return value;
    }
    void Set(HdDataSourceBaseHandle selections)
    {
        std::lock_guard<std::mutex> lock(mutex);
        value = std::move(selections);
    }

    mutable std::mutex     mutex;
    HdDataSourceBaseHandle value;
};

class RenderThreadCaptureSceneIndex::_PrimSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_PrimSource);

    _PrimSource(HdContainerDataSourceHandle copy, std::shared_ptr<_Selections> selections)
        : _copy(std::move(copy))
        , _selections(std::move(selections))
    {
    }

    TfTokenVector GetNames() override
    {
        TfTokenVector names = _copy ? _copy->GetNames() : TfTokenVector();
        names.erase(
            std::remove(names.begin(), names.end(), HdSelectionsSchemaTokens->selections),
            names.end());
        if (_selections->Get()) {
            names.push_back(HdSelectionsSchemaTokens->selections);
        }
        return names;
    }

    HdDataSourceBaseHandle Get(const TfToken& name) override
    {
        if (name == HdSelectionsSchemaTokens->selections) {
            return _selections->Get();
        }
        return _copy ? _copy->Get(name) : nullptr;
    }

private:
    const HdContainerDataSourceHandle  _copy;
    const std::shared_ptr<_Selections> _selections;
};

RenderThreadCaptureByReference::~RenderThreadCaptureByReference() = default;

RenderThreadCaptureSceneIndexRefPtr
RenderThreadCaptureSceneIndex::New(const HdSceneIndexBaseRefPtr& input)
{
    return TfCreateRefPtr(new RenderThreadCaptureSceneIndex(input));
}

RenderThreadCaptureSceneIndex::RenderThreadCaptureSceneIndex(const HdSceneIndexBaseRefPtr& input)
    : HdSingleInputFilteringSceneIndexBase(input)
{
}

RenderThreadCaptureSceneIndex* RenderThreadCaptureSceneIndex::Find(HdRenderIndex* renderIndex)
{
    if (!renderIndex) {
        return nullptr;
    }
    std::vector<HdSceneIndexBaseRefPtr>   toVisit { renderIndex->GetTerminalSceneIndex() };
    std::unordered_set<HdSceneIndexBase*> visited;
    while (!toVisit.empty()) {
        HdSceneIndexBaseRefPtr sceneIndex = toVisit.back();
        toVisit.pop_back();
        if (!sceneIndex || !visited.insert(get_pointer(sceneIndex)).second) {
            continue;
        }
        if (auto* capture = dynamic_cast<RenderThreadCaptureSceneIndex*>(get_pointer(sceneIndex))) {
            return capture;
        }
        if (auto* filtering = dynamic_cast<HdFilteringSceneIndexBase*>(get_pointer(sceneIndex))) {
            for (const HdSceneIndexBaseRefPtr& input : filtering->GetInputScenes()) {
                toVisit.push_back(input);
            }
        }
    }
    return nullptr;
}

std::vector<HdDataSourceLocator>
RenderThreadCaptureSceneIndex::FindUnlistedNames(const HdContainerDataSourceHandle& primSource)
{
    std::vector<HdDataSourceLocator> unlisted;
    if (!primSource) {
        return unlisted;
    }
    static const TfTokenVector topLevel = [] {
        TfTokenVector names;
        for (const _ProbedSchema& schema : _ProbedSchemas()) {
            names.push_back(schema.name);
        }
        return names;
    }();
    _FindUnlisted(primSource, topLevel, HdDataSourceLocator(), unlisted);

    for (const _ProbedSchema& schema : _ProbedSchemas()) {
        if (!schema.members) {
            continue;
        }
        if (HdContainerDataSourceHandle container
            = HdContainerDataSource::Cast(primSource->Get(schema.name))) {
            _FindUnlisted(container, *schema.members, HdDataSourceLocator(schema.name), unlisted);
        }
    }
    return unlisted;
}

namespace {

// Reads on this thread that call upstream under a capture's lock.
thread_local int _upstreamReadsOnThisThread = 0;

// Counts a read that calls upstream under the capture's lock.
class _UpstreamRead
{
public:
    explicit _UpstreamRead(std::atomic<int>& count)
        : _count(count)
    {
        _count.fetch_add(1, std::memory_order_acq_rel);
        ++_upstreamReadsOnThisThread;
    }
    ~_UpstreamRead()
    {
        --_upstreamReadsOnThisThread;
        _count.fetch_sub(1, std::memory_order_acq_rel);
    }

    _UpstreamRead(const _UpstreamRead&) = delete;
    _UpstreamRead& operator=(const _UpstreamRead&) = delete;

private:
    std::atomic<int>& _count;
};

} // namespace

void RenderThreadCaptureSceneIndex::Activate()
{
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (IsActive()) {
        return;
    }
    _FindRendererSources();
    // Everything already in the input, since notices that came before this
    // went straight through.
    std::vector<SdfPath> toVisit { SdfPath::AbsoluteRootPath() };
    while (!toVisit.empty()) {
        const SdfPath path = toVisit.back();
        toVisit.pop_back();
        if (_IsRendererPrim(path, _RendererSourcePrim(path).primType)) {
            _StoreRendererPrim(path);
        } else {
            _Insert(path);
        }
        for (const SdfPath& child : _GetInputSceneIndex()->GetChildPrimPaths(path)) {
            toVisit.push_back(child);
        }
    }
    _active.store(true, std::memory_order_release);
}

void RenderThreadCaptureSceneIndex::Hold()
{
    if (!IsActive()) {
        return;
    }
    _ReceiveDeferred();
    const auto start = std::chrono::steady_clock::now();
    {
        _UpstreamRead                       read(_upstreamReads);
        std::unique_lock<std::shared_mutex> lock(_mutex);
        _CopyPending();
        _holding.store(true, std::memory_order_release);
    }
    RenderThreadStats::instance().RecordCaptureHold(_MicrosSince(start));
    // Those the copy caused join the held queue.
    _ReceiveDeferred();
}

void RenderThreadCaptureSceneIndex::Release()
{
    if (!IsActive() || !_holding.load(std::memory_order_acquire)) {
        return;
    }
    const auto           start = std::chrono::steady_clock::now();
    std::vector<_Notice> held;
    {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        _holding.store(false, std::memory_order_release);
        held.swap(_held);
    }
    // One at a time, so a downstream reader handling a notice sees the copy as of
    // that notice rather than of a later one.
    for (const _Notice& notice : held) {
        {
            std::unique_lock<std::shared_mutex> lock(_mutex);
            std::visit([this](const auto& entries) { _Apply(entries); }, notice);
        }
        _Forward(notice);
    }
    RenderThreadStats::instance().RecordCaptureRelease(_MicrosSince(start));
    _ReceiveDeferred();
}

HdSceneIndexPrim RenderThreadCaptureSceneIndex::GetPrim(const SdfPath& primPath) const
{
    if (!IsActive()) {
        return _GetInputSceneIndex()->GetPrim(primPath);
    }
    if (_holding.load(std::memory_order_acquire)) {
        // Hold() copied everything pending and nothing writes until Release().
        const auto it = _table.find(primPath);
        return it == _table.end() ? HdSceneIndexPrim() : it->second.prim;
    }
    std::shared_lock<std::shared_mutex> lock(_mutex);
    const auto                          it = _table.find(primPath);
    if (it == _table.end()) {
        return HdSceneIndexPrim();
    }
    std::lock_guard<std::mutex> copyLock(_CopyLock(primPath));
    if (it->second.pending) {
        _UpstreamRead read(_upstreamReads);
        _Copy(primPath, it->second);
    }
    return it->second.prim;
}

SdfPathVector RenderThreadCaptureSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    if (!IsActive()) {
        return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }
    std::shared_lock<std::shared_mutex> lock(_mutex, std::defer_lock);
    if (!_holding.load(std::memory_order_acquire)) {
        lock.lock();
    }
    SdfPathVector result;
    auto          it = _table.find(primPath);
    if (it == _table.end()) {
        return result;
    }
    // Depth first, so the next entry is the first child.
    ++it;
    while (it != _table.end() && it->first.GetParentPath() == primPath) {
        result.push_back(it->first);
        it = it.GetNextSubtree();
    }
    return result;
}

template <class Entries, class StoreRenderer, class Send>
void RenderThreadCaptureSceneIndex::_Receive(
    const Entries& entries,
    StoreRenderer  storeRenderer,
    Send           send)
{
    if (!IsActive()) {
        send(entries);
        return;
    }
    Entries renderer;
    Entries others;
    bool    anyRenderer = false;
    bool    held = false;
    {
        // A thread reading under the lock may already own it, and must not try
        // to take it again.
        std::unique_lock<std::shared_mutex> lock(_mutex, std::defer_lock);
        if (_upstreamReadsOnThisThread > 0 || !lock.try_lock()) {
            if (_upstreamReadsOnThisThread > 0
                || _upstreamReads.load(std::memory_order_acquire) > 0) {
                std::lock_guard<std::mutex> deferredLock(_deferredMutex);
                _deferred.emplace_back(entries);
                return;
            }
            lock.lock();
        }
        if constexpr (std::is_same_v<Entries, HdSceneIndexObserver::DirtiedPrimEntries>) {
            _UpstreamRead read(_upstreamReads);
            _UpdateSelections(entries);
        }
        anyRenderer = std::any_of(entries.begin(), entries.end(), [this](const auto& entry) {
            return _IsRendererPrim(entry);
        });
        if (anyRenderer) {
            for (const auto& entry : entries) {
                (_IsRendererPrim(entry) ? renderer : others).push_back(entry);
            }
            storeRenderer(renderer);
        }
        const Entries& rest = anyRenderer ? others : entries;
        if (!rest.empty()) {
            held = _holding.load(std::memory_order_acquire);
            if (held) {
                _held.emplace_back(rest);
            } else {
                _Apply(rest);
            }
        }
    }
    const Entries& rest = anyRenderer ? others : entries;
    if (!renderer.empty()) {
        send(renderer);
    }
    if (!rest.empty() && !held) {
        send(rest);
    }
    _ReceiveDeferred();
}

void RenderThreadCaptureSceneIndex::_ReceiveDeferred()
{
    for (;;) {
        std::vector<_Notice> deferred;
        {
            std::lock_guard<std::mutex> deferredLock(_deferredMutex);
            if (_deferred.empty()) {
                return;
            }
            deferred.swap(_deferred);
        }
        for (const _Notice& notice : deferred) {
            if (const auto* added = std::get_if<HdSceneIndexObserver::AddedPrimEntries>(&notice)) {
                _PrimsAdded(*this, *added);
            } else if (const auto* removed = std::get_if<HdSceneIndexObserver::RemovedPrimEntries>(&notice)) {
                _PrimsRemoved(*this, *removed);
            } else if (const auto* dirtied = std::get_if<HdSceneIndexObserver::DirtiedPrimEntries>(&notice)) {
                _PrimsDirtied(*this, *dirtied);
            }
        }
    }
}

void RenderThreadCaptureSceneIndex::_PrimsAdded(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    _Receive(
        entries,
        [this](const HdSceneIndexObserver::AddedPrimEntries& renderer) {
            for (const auto& entry : renderer) {
                _StoreRendererPrim(entry.primPath);
            }
        },
        [this](const HdSceneIndexObserver::AddedPrimEntries& toSend) { _SendPrimsAdded(toSend); });
}

void RenderThreadCaptureSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    _Receive(
        entries,
        [this](const HdSceneIndexObserver::RemovedPrimEntries& renderer) { _Apply(renderer); },
        [this](const HdSceneIndexObserver::RemovedPrimEntries& toSend) { _SendPrimsRemoved(toSend); });
}

void RenderThreadCaptureSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    // The renderer's prims are held by reference, so they already serve the change.
    _Receive(
        entries,
        [](const HdSceneIndexObserver::DirtiedPrimEntries&) {},
        [this](const HdSceneIndexObserver::DirtiedPrimEntries& toSend) { _SendPrimsDirtied(toSend); });
}

void RenderThreadCaptureSceneIndex::_Apply(const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    for (const auto& entry : entries) {
        _Insert(entry.primPath);
        // Re-adding a prim replaces it, so it is copied whole.
        _table[entry.primPath].copied = false;
    }
}

void RenderThreadCaptureSceneIndex::_Apply(const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    for (const auto& entry : entries) {
        _table.erase(entry.primPath);
        for (SdfPath ancestor = entry.primPath.GetParentPath();
             !ancestor.IsEmpty() && !ancestor.IsAbsoluteRootPath();
             ancestor = ancestor.GetParentPath()) {
            const auto it = _table.find(ancestor);
            if (it == _table.end() || !it->second.implicit) {
                break;
            }
            // Depth first, so the next entry is the first child, if any.
            auto next = it;
            ++next;
            if (next != _table.end() && next->first.GetParentPath() == ancestor) {
                break;
            }
            _table.erase(it);
        }
    }
}

void RenderThreadCaptureSceneIndex::_Apply(const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    for (const auto& entry : entries) {
        const auto it = _table.find(entry.primPath);
        if (it == _table.end()) {
            continue;
        }
        // _UpdateSelections already gave a copied prim its selections.
        const HdDataSourceLocatorSet dirty = it->second.copied && it->second.selections
            ? _WithoutSelections(entry.dirtyLocators)
            : entry.dirtyLocators;
        if (dirty.IsEmpty()) {
            continue;
        }
        it->second.dirty.insert(dirty);
        _MarkPending(it->second, entry.primPath);
    }
}

void RenderThreadCaptureSceneIndex::_UpdateSelections(
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    for (const auto& entry : entries) {
        if (!_NamesSelections(entry.dirtyLocators)) {
            continue;
        }
        const auto it = _table.find(entry.primPath);
        // Not copied yet, the copy will read them.
        if (it == _table.end() || !it->second.copied || !it->second.selections) {
            continue;
        }
        it->second.selections->Set(
            _CopySelections(_GetInputSceneIndex()->GetPrim(entry.primPath).dataSource));
    }
}

void RenderThreadCaptureSceneIndex::_Insert(const SdfPath& path)
{
    // SdfPathTable creates missing ancestors itself; they exist in the input
    // too, so they are copied like any other prim.
    std::vector<SdfPath> missing { path };
    for (SdfPath ancestor = path.GetParentPath(); !ancestor.IsEmpty();
         ancestor = ancestor.GetParentPath()) {
        if (_table.find(ancestor) != _table.end()) {
            break;
        }
        missing.push_back(ancestor);
    }
    for (const SdfPath& p : missing) {
        _Entry& entry = _table[p];
        entry.implicit = p != path;
        _MarkPending(entry, p);
    }
    _table[path].implicit = false;
}

void RenderThreadCaptureSceneIndex::_MarkPending(_Entry& entry, const SdfPath& path)
{
    if (!entry.pending) {
        entry.pending = true;
        _pending.push_back(path);
    }
}

void RenderThreadCaptureSceneIndex::_Copy(const SdfPath& path, _Entry& entry) const
{
    const HdSceneIndexPrim live = _GetInputSceneIndex()->GetPrim(path);
    if (!entry.selections) {
        entry.selections = std::make_shared<_Selections>();
    }
    const bool isLight = HdPrimTypeIsLight(live.primType);
    if (!entry.copied || entry.prim.primType != live.primType) {
        entry.copy = isLight ? _CopyLightPrim(nullptr, live.dataSource)
                             : _CopyContainer(live.dataSource);
        entry.selections->Set(_CopySelections(live.dataSource));
        _WarnUnlistedNames(path, live);
    } else if (!entry.dirty.IsEmpty()) {
        if (isLight && entry.dirty.Contains(HdDataSourceLocator::EmptyLocator())) {
            entry.copy = _CopyLightPrim(nullptr, live.dataSource);
        } else if (isLight && entry.dirty.Intersects(HdLightSchema::GetDefaultLocator())) {
            const HdDataSourceLocatorSet rest = _WithoutLight(entry.dirty);
            entry.copy = _CopyLightPrim(
                rest.IsEmpty() ? entry.copy : _Recopy(entry.copy, live.dataSource, rest),
                live.dataSource);
        } else {
            entry.copy = _Recopy(entry.copy, live.dataSource, entry.dirty);
        }
    }
    entry.prim.primType = live.primType;
    entry.prim.dataSource = entry.copy || entry.selections->Get()
        ? _PrimSource::New(entry.copy, entry.selections)
        : nullptr;
    entry.dirty = HdDataSourceLocatorSet();
    entry.copied = true;
    entry.pending = false;
    RenderThreadStats::instance().RecordPrimCaptured();
}

bool RenderThreadCaptureSceneIndex::_IsRendererPrim(const SdfPath& path, const TfToken& primType) const
{
#if PXR_VERSION >= 2505
    const bool isTask = primType == HdPrimTypeTokens->task;
#else
    // Tasks are not scene index prims before USD 25.05.
    const bool isTask = false;
#endif
    if (!isTask && primType != HdPrimTypeTokens->renderBuffer) {
        return false;
    }
    return std::any_of(_rendererRoots.begin(), _rendererRoots.end(), [&path](const SdfPath& root) {
        return path.HasPrefix(root);
    });
}

bool RenderThreadCaptureSceneIndex::_IsRendererPrim(
    const HdSceneIndexObserver::AddedPrimEntry& entry) const
{
    return _IsRendererPrim(entry.primPath, entry.primType);
}

bool RenderThreadCaptureSceneIndex::_IsRendererPrim(
    const HdSceneIndexObserver::RemovedPrimEntry& entry) const
{
    if (_rendererRoots.empty()) {
        return false;
    }
    const auto it = _table.find(entry.primPath);
    return it != _table.end() && _IsRendererPrim(entry.primPath, it->second.prim.primType);
}

bool RenderThreadCaptureSceneIndex::_IsRendererPrim(
    const HdSceneIndexObserver::DirtiedPrimEntry& entry) const
{
    if (_rendererRoots.empty()) {
        return false;
    }
    const auto it = _table.find(entry.primPath);
    return it != _table.end() && _IsRendererPrim(entry.primPath, it->second.prim.primType);
}

HdSceneIndexPrim RenderThreadCaptureSceneIndex::_RendererSourcePrim(const SdfPath& path) const
{
    const bool underRoot = std::any_of(_rendererRoots.begin(), _rendererRoots.end(),
        [&path](const SdfPath& root) { return path.HasPrefix(root); });
    if (!underRoot) {
        return HdSceneIndexPrim();
    }
    for (const TfWeakPtr<HdRetainedSceneIndex>& source : _rendererSources) {
        if (!source) {
            continue;
        }
        HdSceneIndexPrim prim = source->GetPrim(path);
        if (prim.dataSource || !prim.primType.IsEmpty()) {
            return prim;
        }
    }
    return HdSceneIndexPrim();
}

void RenderThreadCaptureSceneIndex::_StoreRendererPrim(const SdfPath& path)
{
    _Insert(path);
    _Entry& entry = _table[path];
    entry.prim = _RendererSourcePrim(path);
    entry.copy = nullptr;
    entry.selections = nullptr;
    entry.dirty = HdDataSourceLocatorSet();
    entry.copied = true;
    entry.pending = false;
}

void RenderThreadCaptureSceneIndex::_FindRendererSources()
{
    _rendererSources.clear();
    _rendererRoots.clear();
    std::vector<HdSceneIndexBaseRefPtr> toVisit { _GetInputSceneIndex() };
    while (!toVisit.empty()) {
        HdSceneIndexBaseRefPtr sceneIndex = toVisit.back();
        toVisit.pop_back();
        auto* filtering = dynamic_cast<HdFilteringSceneIndexBase*>(get_pointer(sceneIndex));
        if (!filtering) {
            continue;
        }
        const std::vector<HdSceneIndexBaseRefPtr> inputs = filtering->GetInputScenes();
        if (!dynamic_cast<HdMergingSceneIndex*>(filtering)) {
            toVisit.insert(toVisit.end(), inputs.begin(), inputs.end());
            continue;
        }
        for (const HdSceneIndexBaseRefPtr& input : inputs) {
            if (HdRetainedSceneIndexRefPtr retained
                = TfDynamic_cast<HdRetainedSceneIndexRefPtr>(input)) {
                _rendererSources.emplace_back(retained);
                const SdfPathVector roots
                    = retained->GetChildPrimPaths(SdfPath::AbsoluteRootPath());
                _rendererRoots.insert(_rendererRoots.end(), roots.begin(), roots.end());
            }
        }
        return;
    }
}

std::mutex& RenderThreadCaptureSceneIndex::_CopyLock(const SdfPath& path) const
{
    return _copyLocks[SdfPath::Hash()(path) % std::size(_copyLocks)];
}

void RenderThreadCaptureSceneIndex::_CopyPending()
{
    // A path can be listed twice: a lazy copy in GetPrim, or a remove and
    // re-add, clears the entry's pending flag but leaves the path here, and
    // the next dirty lists it again. Two tasks must not copy one entry.
    std::sort(_pending.begin(), _pending.end());
    _pending.erase(std::unique(_pending.begin(), _pending.end()), _pending.end());

    std::vector<std::pair<SdfPath, _Entry*>> work;
    work.reserve(_pending.size());
    for (const SdfPath& path : _pending) {
        const auto it = _table.find(path);
        if (it != _table.end() && it->second.pending) {
            work.emplace_back(path, &it->second);
        }
    }
    _pending.clear();

    // In parallel, as Hydra's sync would pull them. Each task writes only its own
    // entry, and the table itself does not change while this runs.
    WorkParallelForN(work.size(), [this, &work](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            _Copy(work[i].first, *work[i].second);
        }
    });
}

void RenderThreadCaptureSceneIndex::_Forward(const _Notice& notice)
{
    if (const auto* added = std::get_if<HdSceneIndexObserver::AddedPrimEntries>(&notice)) {
        _SendPrimsAdded(*added);
    } else if (const auto* removed = std::get_if<HdSceneIndexObserver::RemovedPrimEntries>(&notice)) {
        _SendPrimsRemoved(*removed);
    } else if (const auto* dirtied = std::get_if<HdSceneIndexObserver::DirtiedPrimEntries>(&notice)) {
        _SendPrimsDirtied(*dirtied);
    }
}

namespace {

HdSceneIndexBaseRefPtr _AppendCaptureSceneIndex(
    const std::string&,
    const HdSceneIndexBaseRefPtr& inputScene,
    const HdContainerDataSourceHandle&)
{
    return RenderThreadCaptureSceneIndex::New(inputScene);
}

} // namespace

void RegisterRenderThreadCaptureSceneIndexPlugin()
{
    // Survives unloading the mayaHydra plugin along with this library (see below),
    // so a reload does not register a second entry.
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    // The registry keeps the callback, a function in this library, for the rest
    // of the process, and every render index created after this calls it,
    // including mayaUsd's. Plug never unloads a library it loaded, so loading
    // this one through it keeps the callback valid once mayaHydra is unloaded.
    // A plugin id instead of a callback would need a loadWithRenderer entry in
    // plugInfo.json to survive the default (hybrid) ordering policy, and that
    // would load this library with every renderer in every session.
    if (PlugPluginPtr plugin = PlugRegistry::GetInstance().GetPluginWithName("mayaHydraLib")) {
        plugin->Load();
    } else {
        TF_WARN("The mayaHydraLib plugin is not registered, so the render thread "
                "capture cannot be registered either. The render thread will read "
                "the live scene.");
        return;
    }

    // An empty renderer name registers it for every renderer.
    HdSceneIndexPluginRegistry::GetInstance().RegisterSceneIndexForRenderer(
        std::string(),
        &_AppendCaptureSceneIndex,
        nullptr,
        RenderThreadCaptureSceneIndex::kInsertionPhase,
        HdSceneIndexPluginRegistry::InsertionOrderAtEnd);
}

} // namespace MAYAHYDRA_NS_DEF
