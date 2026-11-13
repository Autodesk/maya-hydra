//
// Copyright 2026 Autodesk
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
//
// Translates an Arnold aiGaussianSplat node into a Hydra particleField prim carrying the
// UsdVolParticleField3DGaussianSplat primvars that splat-capable render delegates
// consume: positions, scales, orientations, opacities and
// radiance:sphericalHarmonicsCoefficients (degree 0 only).
//
// Unlike OpenVDB volumes, no render delegate reads the splat file itself, so the adapter reads
// the node's PLY file and publishes its contents. Hydra may pull the primvars from a render
// thread, which must not touch Maya; they therefore serve an immutable snapshot that the
// adapter loads on the main thread. Maya callbacks only record that the file changed;
// RefreshData() reloads it during the scene push.
//

#include "gaussianSplatPlyReader.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/adapterRegistry.h>
#include <mayaHydraLib/adapters/mhDirtyNotifier.h>
#include <mayaHydraLib/adapters/shapeAdapter.h>
#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/pxr.h>

#if PXR_VERSION >= 2605

#include <pxr/base/gf/colorSpace.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/MFileObject.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (aiGaussianSplat)
    (positions)
    (scales)
    (orientations)
    (opacities)
    ((shCoefficients, "radiance:sphericalHarmonicsCoefficients"))
    ((shDegree, "radiance:sphericalHarmonicsDegree"))
);
// clang-format on

namespace {

/// GfColorSpaceNames token for an OCIO color space name of Maya's configs, or an empty token
/// when it has no built-in equivalent. Names that already are GfColorSpace names pass through.
TfToken _ToGfColorSpaceName(const std::string& ocioName)
{
    static const std::pair<const char*, TfToken> aliases[] = {
        { "srgb", GfColorSpaceNames->SRGBRec709 },
        { "srgb - texture", GfColorSpaceNames->SRGBRec709 },
        { "srgb encoded rec.709 (srgb)", GfColorSpaceNames->SRGBRec709 },
        { "srgb_tx", GfColorSpaceNames->SRGBRec709 },
        { "linear rec.709 (srgb)", GfColorSpaceNames->LinearRec709 },
        { "scene-linear rec.709-srgb", GfColorSpaceNames->LinearRec709 },
        { "scene-linear rec 709/srgb", GfColorSpaceNames->LinearRec709 },
        { "lin_rec709", GfColorSpaceNames->LinearRec709 },
        { "acescg", GfColorSpaceNames->LinearAP1 },
        { "aces - acescg", GfColorSpaceNames->LinearAP1 },
        { "aces2065-1", GfColorSpaceNames->LinearAP0 },
        { "aces - aces2065-1", GfColorSpaceNames->LinearAP0 },
        { "linear p3-d65", GfColorSpaceNames->LinearP3D65 },
        { "linear rec.2020", GfColorSpaceNames->LinearRec2020 },
        { "gamma 2.2 rec.709", GfColorSpaceNames->G22Rec709 },
        { "gamma 2.2 rec.709 - display", GfColorSpaceNames->G22Rec709 },
        { "raw", GfColorSpaceNames->Raw },
        { "linear", GfColorSpaceNames->Raw },
        { "utility - raw", GfColorSpaceNames->Raw },
    };
    const std::string lower = TfStringToLower(ocioName);
    for (const auto& alias : aliases) {
        if (lower == alias.first) {
            return alias.second;
        }
    }
    const TfToken name(ocioName);
    return GfColorSpace::IsValid(name) ? name : TfToken();
}

} // namespace

/**
 * \brief MayaHydraAiGaussianSplatAdapter translates an Arnold aiGaussianSplat to a Hydra
 * particleField prim holding the splats of the node's PLY file.
 */
class MayaHydraAiGaussianSplatAdapter : public MayaHydraShapeAdapter
{
public:
    MayaHydraAiGaussianSplatAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag)
        : MayaHydraShapeAdapter(mayaHydraSceneIndex->GetPrimPath(dag, false), mayaHydraSceneIndex, dag)
    {
    }

    bool IsSupported() const override
    {
        return GetMayaHydraSceneIndex()->IsRprimTypeSupported(HdPrimTypeTokens->particleField);
    }

    void Populate() override
    {
        if (_isPopulated) {
            return;
        }
        _upAxisRotation = _ReadUpAxisRotation();
        _SetColorSpace(_ReadColorSpace());
        _RefreshSplats(false);
        GetMayaHydraSceneIndex()->InsertPrim(this, HdPrimTypeTokens->particleField, GetID());
        _isPopulated = true;
    }

    void RemovePrim() override
    {
        if (!_isPopulated) {
            return;
        }
        auto* sceneIndex = GetMayaHydraSceneIndex();
        // Render item prims of this shape live below its path; removing the splat prim would
        // take them along, so leave an untyped placeholder while any remain.
        if (sceneIndex->GetChildPrimPaths(GetID()).empty()) {
            sceneIndex->RemovePrim(GetID());
        } else {
            sceneIndex->AddPrims(
                { { GetID(), TfToken(), HdRetainedContainerDataSource::New() } });
        }
        _pendingReload = false;
        _isPopulated = false;
    }

    void RefreshData() override
    {
        if (!std::exchange(_pendingReload, false) || !_CanNotify()) {
            return;
        }
        if (_RefreshSplats(true)) {
            MayaHydra::DirtyNotifier(this).dirtyPrimvars();
        }
    }

    void CreateCallbacks() override
    {
        MStatus status;
        auto    obj = GetNode();
        if (obj != MObject::kNullObj) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
                .Msg(
                    "Creating aiGaussianSplat adapter callbacks for prim (%s).\n",
                    GetID().GetText());

            auto id = MNodeMessage::addAttributeChangedCallback(
                obj, _AttributeChanged, this, &status);
            if (status) {
                AddCallback(id);
            }
        }
        MayaHydraDagAdapter::CreateCallbacks();
    }

    VtValue Get(const TfToken& key) override
    {
        const auto splats = _GetSplats();
        if (!splats) {
            return {};
        }
        if (key == _tokens->positions) {
            return VtValue(splats->positions);
        }
        if (key == _tokens->scales) {
            return VtValue(splats->scales);
        }
        if (key == _tokens->orientations) {
            return VtValue(splats->orientations);
        }
        if (key == _tokens->opacities) {
            return VtValue(splats->opacities);
        }
        if (key == _tokens->shCoefficients) {
            return VtValue(splats->shDc);
        }
        if (key == _tokens->shDegree) {
            return VtValue(0);
        }
        return {};
    }

    // Only the splat primvars: the node's many plugin attributes are not useful to a renderer.
    HdPrimvarDescriptorVector GetPrimvarDescriptors(HdInterpolation interpolation) override
    {
        if (!_GetSplats()) {
            return {};
        }
        if (interpolation == HdInterpolationVertex) {
            return {
                { _tokens->positions, interpolation, HdPrimvarRoleTokens->point },
                { _tokens->scales, interpolation, HdPrimvarRoleTokens->vector },
                { _tokens->orientations, interpolation, HdPrimvarRoleTokens->none },
                { _tokens->opacities, interpolation, HdPrimvarRoleTokens->none },
                { _tokens->shCoefficients, interpolation, HdPrimvarRoleTokens->none },
            };
        }
        if (interpolation == HdInterpolationConstant) {
            return { { _tokens->shDegree, interpolation, HdPrimvarRoleTokens->none } };
        }
        return {};
    }

    // The splat colors are in the node's inputColorSpace.
    TfToken GetPrimvarColorSpace(const TfToken& name) const override
    {
        if (name != _tokens->shCoefficients) {
            return {};
        }
        std::lock_guard<std::mutex> lock(_colorSpaceMutex);
        return _colorSpace;
    }

protected:
    // The file's up axis is turned to Maya's +Y by a rotation about X, ahead of the node's
    // own transform.
    GfMatrix4d _ComputeTransform() const override
    {
        const GfMatrix4d world = MayaHydraShapeAdapter::_ComputeTransform();
        const int        degrees = _upAxisRotation;
        if (degrees == 0) {
            return world;
        }
        return GfMatrix4d().SetRotate(GfRotation(GfVec3d::XAxis(), degrees)) * world;
    }

private:
    using _SplatsPtr = std::shared_ptr<const MayaHydraGaussianSplats>;

    _SplatsPtr _GetSplats() const
    {
        std::lock_guard<std::mutex> lock(_splatsMutex);
        return _splats;
    }

    bool _CanNotify() const
    {
        return _isPopulated && !ShouldSkipHydraUpdates(GetMayaHydraSceneIndex());
    }

    /// Resolved PLY file, or an empty string when the node does not use a file.
    std::string _ResolveFilePath() const
    {
        MayaHydra::DgAccessLock dgLock;

        MFnDependencyNode node(GetNode());
        if (!node.findPlug("useFile", true).asBool()) {
            return {};
        }
        const MString path = node.findPlug("filename", true).asString();
        if (path.length() == 0) {
            return {};
        }
        MFileObject file;
        file.setRawFullName(path);
        file.setResolveMethod(MFileObject::kInputFile);
        const MString resolved = file.resolvedFullName();
        return resolved.length() > 0 ? resolved.asChar() : file.expandedFullName().asChar();
    }

    /// Degrees about X that turn the upAxis field ("Y Up", "-Y Up", "Z Up", "-Z Up") to +Y.
    int _ReadUpAxisRotation() const
    {
        MayaHydra::DgAccessLock dgLock;

        const MPlug plug = MFnDependencyNode(GetNode()).findPlug("upAxis", true);
        if (plug.isNull()) {
            return 0;
        }
        const MString field = MFnEnumAttribute(plug.attribute()).fieldName(plug.asShort());
        const char*   name = field.asChar();
        const bool    negative = name[0] == '-';
        const char    axis = negative ? name[1] : name[0];
        if (axis == 'Z' || axis == 'z') {
            return negative ? 90 : -90;
        }
        if (axis == 'Y' || axis == 'y') {
            return negative ? 180 : 0;
        }
        return 0;
    }

    /// Main thread only. GfColorSpaceNames token of the inputColorSpace attribute, which holds
    /// an OCIO name and defaults to sRGB like Arnold. Unknown names leave colors untransformed.
    TfToken _ReadColorSpace()
    {
        std::string ocioName;
        {
            MayaHydra::DgAccessLock dgLock;

            const MPlug plug = MFnDependencyNode(GetNode()).findPlug("inputColorSpace", true);
            if (!plug.isNull()) {
                ocioName = plug.asString().asChar();
            }
        }
        if (ocioName.empty()) {
            return GfColorSpaceNames->SRGBRec709;
        }
        const TfToken name = _ToGfColorSpaceName(ocioName);
        if (!name.IsEmpty()) {
            return name;
        }
        if (ocioName != _warnedColorSpace) {
            _warnedColorSpace = ocioName;
            TF_WARN(
                "aiGaussianSplat %s: color space '%s' is not supported in the viewport; "
                "colors are shown untransformed.",
                GetDagPath().partialPathName().asChar(),
                ocioName.c_str());
        }
        return GfColorSpaceNames->Raw;
    }

    /// Returns whether the color space changed.
    bool _SetColorSpace(const TfToken& colorSpace)
    {
        std::lock_guard<std::mutex> lock(_colorSpaceMutex);
        return std::exchange(_colorSpace, colorSpace) != colorSpace;
    }

    /// Main thread only. Loads the splats of the current file; unless \p force, keeps the loaded
    /// ones when the file did not change. Returns whether the published splats changed.
    bool _RefreshSplats(bool force)
    {
        const std::string path = _ResolveFilePath();
        if (!force && path == _loadedPath) {
            return false;
        }
        const bool hadSplats = _GetSplats() != nullptr;
        _loadedPath = path;

        _SplatsPtr splats;
        if (!path.empty()) {
            auto        loaded = std::make_shared<MayaHydraGaussianSplats>();
            std::string error;
            if (MayaHydraReadGaussianSplatPly(path, *loaded, error)) {
                splats = std::move(loaded);
            } else {
                TF_WARN(
                    "aiGaussianSplat %s: cannot read '%s': %s",
                    GetDagPath().partialPathName().asChar(),
                    path.c_str(),
                    error.c_str());
            }
        }
        {
            std::lock_guard<std::mutex> lock(_splatsMutex);
            _splats = std::move(splats);
        }
        return hadSplats || _GetSplats() != nullptr;
    }

    void _RequestReload()
    {
        if (!_CanNotify()) {
            return;
        }
        _pendingReload = true;
        GetMayaHydraSceneIndex()->RebuildAdapterOnIdle(
            GetID(), MayaHydraSceneIndex::RebuildFlagData);
    }

    static void _AttributeChanged(
        MNodeMessage::AttributeMessage msg,
        MPlug&                         plug,
        MPlug&                         otherPlug,
        void*                          clientData)
    {
        TF_UNUSED(otherPlug);
        if (!(msg & MNodeMessage::kAttributeSet)) {
            return;
        }
        auto*         adapter = reinterpret_cast<MayaHydraAiGaussianSplatAdapter*>(clientData);
        const MString name = MayaHydra::GetTopPlug(plug).partialName(
            false, false, false, false, false, true);
        if (name == "filename" || name == "useFile") {
            adapter->_RequestReload();
        } else if (name == "upAxis") {
            const int degrees = adapter->_ReadUpAxisRotation();
            if (degrees != adapter->_upAxisRotation && adapter->_CanNotify()) {
                adapter->_upAxisRotation = degrees;
                MayaHydraDagAdapter::DirtyTransformIfVisible(adapter);
            }
        } else if (name == "inputColorSpace") {
            if (adapter->_SetColorSpace(adapter->_ReadColorSpace()) && adapter->_CanNotify()) {
                MayaHydra::DirtyNotifier(adapter).dirtyPrimvar(_tokens->shCoefficients);
            }
        }
    }

    mutable std::mutex _splatsMutex;
    _SplatsPtr         _splats;
    std::string        _loadedPath;
    bool               _pendingReload { false };
    std::atomic<int>   _upAxisRotation { 0 };
    mutable std::mutex _colorSpaceMutex;
    TfToken            _colorSpace;
    std::string        _warnedColorSpace;
};

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraAiGaussianSplatAdapter, TfType::Bases<MayaHydraShapeAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, aiGaussianSplat)
{
    MayaHydraAdapterRegistry::RegisterShapeAdapter(
        _tokens->aiGaussianSplat,
        [](MayaHydraSceneIndex* mayaHydraSceneIndex,
           const MDagPath&      dag) -> MayaHydraShapeAdapterPtr {
            return MayaHydraShapeAdapterPtr(
                new MayaHydraAiGaussianSplatAdapter(mayaHydraSceneIndex, dag));
        });
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_VERSION >= 2605
