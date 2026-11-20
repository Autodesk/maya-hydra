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

// VtVisitValue dispatches every Vt value type, so all of them must be complete.
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
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/primOriginSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/purposeSchema.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>
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

struct _MultisampledCopy
{
    std::vector<HdSampledDataSource::Time>* times;
    const std::vector<VtValue>*             values;

    template <class T>
    HdSampledDataSourceHandle operator()(const T&) const
    {
        // Not std::vector, which has no data() for bool.
        std::unique_ptr<T[]> typed(new T[values->size()]);
        for (size_t i = 0; i < values->size(); ++i) {
            if (!(*values)[i].IsHolding<T>()) {
                return nullptr;
            }
            typed[i] = (*values)[i].UncheckedGet<T>();
        }
        return HdRetainedTypedMultisampledDataSource<T>::New(
            values->size(), times->data(), typed.get());
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
        // with the live value rather than a sample; keep it exact.
        const auto zero = std::find_if(times.begin(), times.end(), [](HdSampledDataSource::Time t) {
            return std::fabs(t) < kSampleTimeEpsilon;
        });
        if (zero != times.end()) {
            *zero = 0.0f;
        } else {
            times.insert(std::upper_bound(times.begin(), times.end(), 0.0f), 0.0f);
        }
        std::vector<VtValue> values;
        values.reserve(times.size());
        for (const HdSampledDataSource::Time t : times) {
            values.push_back(t == 0.0f ? value : ds->GetValue(t));
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

// Whether \p ds gives a reader anything: a container, a vector or a non-empty value.
bool _Serves(const HdDataSourceBaseHandle& ds)
{
    if (!ds || std::dynamic_pointer_cast<HdBlockDataSource>(ds)) {
        return false;
    }
    if (HdSampledDataSourceHandle sampled = HdSampledDataSource::Cast(ds)) {
        return !sampled->GetValue(0.0f).IsEmpty();
    }
    return true;
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
    const std::vector<HdDataSourceLocator> unlisted
        = MAYAHYDRA_NS_DEF::RenderThreadCaptureSceneIndex::FindUnlistedNames(prim.dataSource);
    if (unlisted.empty()) {
        return;
    }
    // Once per prim type and name: the cause is a data source class, not a prim.
    static std::mutex                      warnedMutex;
    static std::unordered_set<std::string> warned;
    for (const HdDataSourceLocator& locator : unlisted) {
        const std::string key = prim.primType.GetString() + " " + locator.GetString();
        {
            std::lock_guard<std::mutex> lock(warnedMutex);
            if (!warned.insert(key).second) {
                continue;
            }
        }
        TF_WARN(
            "%s (%s) serves '%s' but its container does not list it in GetNames(), so "
            "the render thread's copy of the scene drops it.",
            primPath.GetText(),
            prim.primType.GetText(),
            locator.GetString().c_str());
    }
}

} // namespace

namespace MAYAHYDRA_NS_DEF {

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

void RenderThreadCaptureSceneIndex::Activate()
{
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (IsActive()) {
        return;
    }
    // Everything already in the input, since notices that came before this
    // went straight through.
    std::vector<SdfPath> toVisit { SdfPath::AbsoluteRootPath() };
    while (!toVisit.empty()) {
        const SdfPath path = toVisit.back();
        toVisit.pop_back();
        _Insert(path);
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
    const auto start = std::chrono::steady_clock::now();
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _CopyPending();
    _holding.store(true, std::memory_order_release);
    RenderThreadStats::instance().RecordCaptureHold(_MicrosSince(start));
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

void RenderThreadCaptureSceneIndex::_PrimsAdded(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    if (!IsActive()) {
        _SendPrimsAdded(entries);
        return;
    }
    {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        if (_holding.load(std::memory_order_acquire)) {
            _held.emplace_back(entries);
            return;
        }
        _Apply(entries);
    }
    _SendPrimsAdded(entries);
}

void RenderThreadCaptureSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    if (!IsActive()) {
        _SendPrimsRemoved(entries);
        return;
    }
    {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        if (_holding.load(std::memory_order_acquire)) {
            _held.emplace_back(entries);
            return;
        }
        _Apply(entries);
    }
    _SendPrimsRemoved(entries);
}

void RenderThreadCaptureSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    if (!IsActive()) {
        _SendPrimsDirtied(entries);
        return;
    }
    {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        if (_holding.load(std::memory_order_acquire)) {
            _held.emplace_back(entries);
            return;
        }
        _Apply(entries);
    }
    _SendPrimsDirtied(entries);
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
    }
}

void RenderThreadCaptureSceneIndex::_Apply(const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    for (const auto& entry : entries) {
        const auto it = _table.find(entry.primPath);
        if (it == _table.end()) {
            continue;
        }
        it->second.dirty.insert(entry.dirtyLocators);
        _MarkPending(it->second, entry.primPath);
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
        _MarkPending(_table[p], p);
    }
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
    if (!entry.copied || entry.prim.primType != live.primType) {
        entry.prim = { live.primType, _CopyContainer(live.dataSource) };
        _WarnUnlistedNames(path, live);
    } else if (!entry.dirty.IsEmpty()) {
        entry.prim.dataSource = _Recopy(entry.prim.dataSource, live.dataSource, entry.dirty);
    }
    entry.dirty = HdDataSourceLocatorSet();
    entry.copied = true;
    entry.pending = false;
    RenderThreadStats::instance().RecordPrimCaptured();
}

std::mutex& RenderThreadCaptureSceneIndex::_CopyLock(const SdfPath& path) const
{
    return _copyLocks[SdfPath::Hash()(path) % std::size(_copyLocks)];
}

void RenderThreadCaptureSceneIndex::_CopyPending()
{
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
