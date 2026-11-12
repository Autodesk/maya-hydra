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

#include "renderGlobals.h"

#include "renderOverride.h"
#include "renderSettingsUiConfig.h"

#include <mayaHydraLib/tokens.h>

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/pxr.h>
#include <pxr/usd/usdRender/settings.h>

#include <maya/MFnDependencyNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <vector>

// This file is where we build the UI and expose to MEL the global parameters from this plug-in and
// the parameters from the chosen render delegate.

// Configurations where the Outline selection-highlight mode is not offered (the enum lists
// "Legacy Selection" only):
//  - USD <= 24.11: HgiGL corrupts the integer prim ids the outline compute shader samples.
//  - macOS: not supported.
// Renderers other than Storm and Flash never use it either
// (MtohRenderOverride::_RendererSupportsOutline()).
// Keep in sync with outlineSelectionHighlight's default in renderGlobals.h,
// MayaHydraBaseTestCase.outlineSelectionHighlightSupported() in test/testUtils/mtohUtils.py and
// MAYAHYDRA_OUTLINE_SUPPORTED in test/lib/mayaUsd/render/mayaToHydra/CMakeLists.txt.
#if PXR_VERSION <= 2411 || defined(__APPLE__)
#define MAYAHYDRA_NO_OUTLINE_SELECTION_HIGHLIGHT
#endif

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to 
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (defaultRenderGlobals)
    (mayaHydraProxyPurpose)
    (mayaHydraRenderPurpose)
    (mayaHydraGuidePurpose)
    (mtohTextureMemoryPerTexture)
    (mtohMotionSampleStart)
    (mtohMotionSampleEnd)
);
// clang-format on

namespace {

// Pre-amble that all option-boxes will need
//
constexpr auto _renderOverride_PreAmble = R"mel(
global proc mtohRenderOverride_ApplySetting(string $renderer, string $attr, string $node) {
    // This exists as a global function for the difference in how it is invoked from editorTemplate/AE or option-boxes
    mayaHydra -r $renderer -updateRenderGlobals $attr;
    refresh -f;
}
// Label column width of the option box being built. Each renderer's OptionBox proc sets it from
// its longest label (see MtohRenderGlobals::BuildOptionsMenu) before building its controls.
global int $gMtohRenderOverrideLabelWidth = 220;
// Widen the label column of a control created by attrControlGrp so long labels are not clipped.
// attrControlGrp returns a rowGroupLayout whose short name starts with the command that made it
// (checkBoxGrp, attrFieldSliderGrp, attrEnumOptionMenuGrp, textFieldGrp, attrColorSliderGrp...).
global proc mtohRenderOverride_FitLabel(string $ctrl) {
    global int $gMtohRenderOverrideLabelWidth;
    string $parts[];
    tokenize $ctrl "|" $parts;
    if (size($parts) == 0) {
        return;
    }
    string $cmd = `match "^[A-Za-z]+" $parts[size($parts) - 1]`;
    if ($cmd != "") {
        catchQuiet(eval($cmd + " -e -columnWidth 1 " + $gMtohRenderOverrideLabelWidth + " \"" + $ctrl + "\""));
    }
}
global proc mtohRenderOverride_AddAttribute(string $renderer, string $description, string $attr, string $label, int $fromAE) {
    string $command = "mtohRenderOverride_ApplySetting " + $renderer + " " + $attr;
    if (!$fromAE) {
        $command = $command + " defaultRenderGlobals";
        string $ctrl = `attrControlGrp -label $label -attribute ("defaultRenderGlobals." + $attr) -changeCommand $command -annotation $description`;
        mtohRenderOverride_FitLabel($ctrl);
    } else {
        editorTemplate -label $label -adc $attr $command -annotation $description;
    }
}
// Collapsible category: a frameLayout in the option box, a layout in the Attribute Editor.
global proc mtohRenderOverride_BeginGroup(string $label, int $collapse, int $fromAE) {
    if (!$fromAE) {
        frameLayout -label $label -collapsable true -collapse $collapse -marginWidth 4 -marginHeight 2;
        columnLayout -adjustableColumn true;
    } else {
        editorTemplate -beginLayout $label -collapse $collapse;
    }
}
global proc mtohRenderOverride_EndGroup(int $fromAE) {
    if (!$fromAE) {
        setParent ..;
        setParent ..;
    } else {
        editorTemplate -endLayout;
    }
}

// Settings shared by every render delegate (refine level, display purpose). Shown once in the Attribute Editor rather than under
// each renderer, so the same attribute is not added to the editorTemplate several times. The
// renderer "mayaHydra" addresses all renderers (see MtohViewCmd).
global proc mtohRenderOverride_GlobalOptions(int $fromAE) {
    mtohRenderOverride_BeginGroup("All renderers", 0, $fromAE);
        mtohRenderOverride_AddAttribute("mayaHydra", "Subdivision refine level (mayaHydraRefinementLevel)", "mayaHydraRefinementLevel", "Refine level", $fromAE);
        mtohRenderOverride_AddAttribute("mayaHydra", "Draw prims with the proxy purpose (mayaHydraProxyPurpose)", "mayaHydraProxyPurpose", "Proxy purpose", $fromAE);
        mtohRenderOverride_AddAttribute("mayaHydra", "Draw prims with the render purpose (mayaHydraRenderPurpose)", "mayaHydraRenderPurpose", "Render purpose", $fromAE);
        mtohRenderOverride_AddAttribute("mayaHydra", "Draw prims with the guide purpose (mayaHydraGuidePurpose)", "mayaHydraGuidePurpose", "Guide purpose", $fromAE);
    mtohRenderOverride_EndGroup($fromAE);
}

global proc mtohRenderOverride_AEAttributesCallback(string $nodeName) {
    if (`nodeType $nodeName` != "renderGlobals") {
        return;
    }

    editorTemplate -beginLayout "Hydra Settings" -collapse 1;
        mtohRenderOverride_GlobalOptions(1);
        for ($renderer in `mayaHydra -lr`) {
            string $displayName = `mayaHydra -getRendererDisplayName -r $renderer`;
            editorTemplate -beginLayout $displayName -collapse 1;
                string $optionsCmd = "{{overridePrefix}}" + $renderer + "Options(1);";
                eval($optionsCmd);
            editorTemplate -endLayout;
        }
    editorTemplate -endLayout;
}

)mel";

constexpr auto _renderOverrideOptionBoxTemplate = R"mel(
global proc {{override}}OptionBox() {
    string $windowName = "{{override}}OptionsWindow";
    if (`window -exists $windowName`) {
        showWindow $windowName;
        return;
    }

    // XXX: Could have an optionVar controlling -userDefaults flag
    //
    mayaHydra -createRenderGlobals -r "{{hydraplugin}}" -userDefaults;

    // Sized to this renderer's longest label, computed when the plugin built this proc.
    global int $gMtohRenderOverrideLabelWidth;
    $gMtohRenderOverrideLabelWidth = {{labelWidth}};
    window -title "Maya to Hydra settings" -widthHeight {{windowWidth}} 640 "{{override}}OptionsWindow";
    scrollLayout -childResizable true;
    columnLayout -adjustableColumn true;

    // Settings shared by all renderers come first, as in the Attribute Editor.
    mtohRenderOverride_GlobalOptions(0);

    frameLayout -label "{{hydraDisplayName}}" -collapsable true;
    columnLayout -adjustableColumn true;
    {{override}}Options(0);
    setParent ..;
    setParent ..;

    setParent ..;
    setParent ..;

    showWindow $windowName;
    // Maya restores a saved window size; widen it if the labels need more room than that.
    if (`window -q -width $windowName` < {{windowWidth}}) {
        window -e -width {{windowWidth}} $windowName;
    }
}
)mel";

static constexpr const char* kMtohNSToken = "_mtohns_";
static const std::string     kMtohRendererPostFix("__");

static MString _MangleColorAttribute(const MString& attrName, unsigned i)
{
    static const MString                kMtohCmptToken("_mtohc_");
    static const std::array<MString, 4> kColorComponents = { "R", "G", "B", "A" };
    if (i < kColorComponents.size()) {
        return attrName + kMtohCmptToken + kColorComponents[i];
    }

    TF_CODING_ERROR("[mayaHydra] Cannot mangle component: %u", i);
    return attrName + kMtohCmptToken + MString("INVALID");
}

static MString _AlphaAttribute(const MString& attrName)
{
    return _MangleColorAttribute(attrName, 3);
}

template <typename HydraType, typename PrefType>
bool _RestoreValue(
    MFnDependencyNode& node,
    const MString&     attrName,
    PrefType (*getter)(const MString&, bool* valid))
{
    bool     valid = false;
    PrefType mayaPref = getter(attrName, &valid);
    if (valid) {
        auto plug = node.findPlug(attrName, true);
        if (!plug.isNull()) {
            plug.setValue(HydraType(mayaPref));
        }
    }
    return valid;
}

void _CreateEnumAttribute(
    MFnDependencyNode&   node,
    const MString&       attrName,
    const TfTokenVector& values,
    const TfToken&       defValue,
    bool                 useUserOptions)
{
    const auto attr = node.attribute(attrName);
    const bool existed = !attr.isNull();
    if (existed) {
        const auto sameOrder = [&attr, &values]() -> bool {
            MStatus          status;
            MFnEnumAttribute eAttr(attr, &status);
            if (!status) {
                return false;
            }
            short id = 0;
            for (const auto& v : values) {
                if (eAttr.fieldName(id++) != v.GetText()) {
                    return false;
                }
            }
            return true;
        };
        if (sameOrder()) {
            return;
        }

        node.removeAttribute(attr);
    }

    MFnEnumAttribute eAttr;
    auto             o = eAttr.create(attrName, attrName);
    short            id = 0;
    for (const auto& v : values) {
        eAttr.addField(v.GetText(), id++);
    }
    eAttr.setDefault(defValue.GetText());
    node.addAttribute(o);

    if (existed || !useUserOptions) {
        return;
    }

    // Enums stored as string to allow re-ordering
    // Why MPlug::setValue doesn't handle this ?
    //
    bool    valid = false;
    TfToken mayaPref(MGlobal::optionVarStringValue(attrName, &valid).asChar());
    if (!valid) {
        return;
    }

    for (int i = 0, n = values.size(); i < n; ++i) {
        if (mayaPref == values[i]) {
            auto plug = node.findPlug(attrName, true);
            if (!plug.isNull()) {
                plug.setValue(i);
            }
            return;
        }
    }
    TF_WARN("[mayaHydra] Cannot restore enum '%s'", mayaPref.GetText());
}

void _CreateEnumAttribute(
    MFnDependencyNode& node,
    const MString&     attrName,
    const TfEnum&      defValue,
    bool               useUserOptions)
{
    std::vector<std::string> names = TfEnum::GetAllNames(defValue);
    TfTokenVector            tokens(names.begin(), names.end());
    return _CreateEnumAttribute(
        node, attrName, tokens, TfToken(TfEnum::GetDisplayName(defValue)), useUserOptions);
}

void _CreateStringAttribute(
    MFnDependencyNode& node,
    const MString&     attrName,
    const std::string& defValue,
    bool               useUserOptions,
    bool               usedasFilename = false)
{

    const auto attr = node.attribute(attrName);
    const bool existed = !attr.isNull();
    if (existed) {
        MStatus           status;
        MFnTypedAttribute tAttr(attr, &status);
        if (status && tAttr.attrType() == MFnData::kString) {
            return;
        }
        node.removeAttribute(attr);
    }

    MFnTypedAttribute tAttr;
    const auto        obj = tAttr.create(attrName, attrName, MFnData::kString);
    if (!defValue.empty()) {
        MFnStringData strData;
        MObject       defObj = strData.create(defValue.c_str());
        tAttr.setDefault(defObj);
    }
    tAttr.setUsedAsFilename(usedasFilename);

    node.addAttribute(obj);

    if (!existed && useUserOptions) {
        _RestoreValue<MString>(node, attrName, MGlobal::optionVarStringValue);
    }
}

template <typename T, typename MayaType>
void _CreateNumericAttribute(
    MFnDependencyNode&                                      node,
    const MString&                                          attrName,
    MFnNumericData::Type                                    type,
    typename std::enable_if<std::is_pod<T>::value, T>::type defValue,
    bool                                                    useUserOptions,
    MayaType (*getter)(const MString&, bool* valid),
    std::function<void(MFnNumericAttribute& nAttr)> postCreate = {})
{

    const auto attr = node.attribute(attrName);
    const bool existed = !attr.isNull();
    if (existed) {
        MStatus             status;
        MFnNumericAttribute nAttr(attr, &status);
        if (status && nAttr.unitType() == type) {
            return;
        }
        node.removeAttribute(attr);
    }

    MFnNumericAttribute nAttr;
    const auto          obj = nAttr.create(attrName, attrName, type);
    nAttr.setDefault(defValue);
    if (postCreate) {
        postCreate(nAttr);
    }
    node.addAttribute(obj);

    if (!existed && useUserOptions) {
        _RestoreValue<T, MayaType>(node, attrName, getter);
    }
}

template <typename T>
void _CreateColorAttribute(
    MFnDependencyNode&                                             node,
    const MString&                                                 attrName,
    T                                                              defValue,
    bool                                                           useUserOptions,
    std::function<bool(MFnNumericAttribute& nAttr, bool doCreate)> alphaOp = {})
{
    const auto attr = node.attribute(attrName);
    if (!attr.isNull()) {
        MStatus             status;
        MFnNumericAttribute nAttr(attr, &status);
        if (status && nAttr.isUsedAsColor() && (!alphaOp || alphaOp(nAttr, false))) {
            return;
        }
        node.removeAttribute(attr);
    }
    MFnNumericAttribute nAttr;
    const auto          o = nAttr.createColor(attrName, attrName);
    nAttr.setDefault(defValue[0], defValue[1], defValue[2]);
    node.addAttribute(o);

    if (alphaOp) {
        alphaOp(nAttr, true);
    }

    if (useUserOptions) {
        for (unsigned i = 0; i < T::dimension; ++i) {
            _RestoreValue<float>(
                node, _MangleColorAttribute(attrName, i), MGlobal::optionVarDoubleValue);
        }
    }
}

void _CreateColorAttribute(
    MFnDependencyNode& node,
    const MString&     attrName,
    GfVec4f            defVec4,
    bool               useUserOptions)
{
    _CreateColorAttribute(
        node,
        attrName,
        GfVec3f(defVec4.data()),
        useUserOptions,
        [&](MFnNumericAttribute& nAttr, bool doCreate) -> bool {
            const MString attrAName = _AlphaAttribute(attrName);
            const auto    attrA = node.attribute(attrAName);
            // If we previously found the color attribute, make sure the Alpha attribute
            // is also a match (MFnNumericData::kFloat), otherwise delete it and signal to re-create
            // the color too.
            if (!doCreate) {
                if (!attrA.isNull()) {
                    MStatus             status;
                    MFnNumericAttribute nAttr(attrA, &status);
                    if (status && nAttr.unitType() == MFnNumericData::kFloat) {
                        return true;
                    }
                    node.removeAttribute(attrA);
                }
                return false;
            }

            const auto o = nAttr.create(attrAName, attrAName, MFnNumericData::kFloat);
            nAttr.setDefault(defVec4[3]);
            node.addAttribute(o);
            return true;
        });
}

void _CreateBoolAttribute(
    MFnDependencyNode& node,
    const MString&     attrName,
    bool               defValue,
    bool               useUserOptions)
{
    _CreateNumericAttribute<bool>(
        node,
        attrName,
        MFnNumericData::kBoolean,
        defValue,
        useUserOptions,
        MGlobal::optionVarIntValue);
}

// A bool that lasts only for the session: never restored from or saved to an optionVar, and not
// storable, so its value reaches neither the user preferences nor a saved scene. Used for the
// script-only profiling switches, which have no UI: a persisted value would come back on every
// launch with nothing to show it is on.
void _CreateSessionOnlyBoolAttribute(
    MFnDependencyNode& node,
    const MString&     attrName,
    bool               defValue)
{
    // Discard a value saved by earlier builds, which persisted these settings.
    if (MGlobal::optionVarExists(attrName)) {
        MGlobal::removeOptionVar(attrName);
    }

    const auto attr = node.attribute(attrName);
    if (!attr.isNull()) {
        MStatus             status;
        MFnNumericAttribute nAttr(attr, &status);
        if (status && nAttr.unitType() == MFnNumericData::kBoolean && !nAttr.isStorable()) {
            return;
        }
        // A storable attribute was created by an earlier build, or loaded from a scene saved by
        // one. Recreate it so its value is dropped along with it.
        node.removeAttribute(attr);
    }

    MFnNumericAttribute nAttr;
    const auto          obj = nAttr.create(attrName, attrName, MFnNumericData::kBoolean);
    nAttr.setDefault(defValue);
    nAttr.setStorable(false);
    node.addAttribute(obj);
}

void _CreateIntAttribute(
    MFnDependencyNode&                              node,
    const MString&                                  attrName,
    int                                             defValue,
    bool                                            useUserOptions,
    std::function<void(MFnNumericAttribute& nAttr)> postCreate = {})
{
    _CreateNumericAttribute<int>(
        node,
        attrName,
        MFnNumericData::kInt,
        defValue,
        useUserOptions,
        MGlobal::optionVarIntValue,
        std::move(postCreate));
}

void _CreateFloatAttribute(
    MFnDependencyNode& node,
    const MString&     attrName,
    float              defValue,
    bool               useUserOptions)
{
    _CreateNumericAttribute<float>(
        node,
        attrName,
        MFnNumericData::kFloat,
        defValue,
        useUserOptions,
        MGlobal::optionVarDoubleValue);
}

template <typename T> void _GetFromPlug(const MPlug& plug, T& out) { assert(false); }

template <> void _GetFromPlug<bool>(const MPlug& plug, bool& out) { out = plug.asBool(); }

template <> void _GetFromPlug<int>(const MPlug& plug, int& out) { out = plug.asInt(); }

template <> void _GetFromPlug<unsigned int>(const MPlug& plug, unsigned int& out)
{
    const int v = plug.asInt();
    out = static_cast<unsigned int>(v < 0 ? 0 : v);
}

template <> void _GetFromPlug<float>(const MPlug& plug, float& out) { out = plug.asFloat(); }

template <> void _GetFromPlug<std::string>(const MPlug& plug, std::string& out)
{
    out = plug.asString().asChar();
}

template <> void _GetFromPlug<TfEnum>(const MPlug& plug, TfEnum& out)
{
    out = TfEnum(out.GetType(), plug.asInt());
}

template <> void _GetFromPlug<SdfAssetPath>(const MPlug& plug, SdfAssetPath& out)
{
    out = SdfAssetPath(std::string(plug.asString().asChar())); //  (out.GetType(), plug.asInt());
}

template <> void _GetFromPlug<TfToken>(const MPlug& plug, TfToken& out)
{
    MObject attribute = plug.attribute();

    if (attribute.hasFn(MFn::kEnumAttribute)) {
        MFnEnumAttribute enumAttr(attribute);
        MString          value = enumAttr.fieldName(plug.asShort());
        out = TfToken(value.asChar());
    } else {
        out = TfToken(plug.asString().asChar());
    }
}

template <typename T> bool _SetOptionVar(const MString& attrName, const T& value)
{
    return MGlobal::setOptionVarValue(attrName, value);
}

bool _SetOptionVar(const MString& attrName, const bool& value)
{
    return _SetOptionVar(attrName, int(value));
}

bool _SetOptionVar(const MString& attrName, const unsigned int& value)
{
    return _SetOptionVar(attrName, static_cast<int>(value));
}

bool _SetOptionVar(const MString& attrName, const float& value)
{
    return _SetOptionVar(attrName, double(value));
}

bool _SetOptionVar(const MString& attrName, const TfToken& value)
{
    return _SetOptionVar(attrName, MString(value.GetText()));
}

bool _SetOptionVar(const MString& attrName, const std::string& value)
{
    return _SetOptionVar(attrName, MString(value.c_str()));
}

bool _SetOptionVar(const MString& attrName, const SdfAssetPath& value)
{
    return _SetOptionVar(attrName, MString(value.GetAssetPath().c_str()));
}

bool _SetOptionVar(const MString& attrName, const TfEnum& value)
{
    return _SetOptionVar(attrName, TfEnum::GetDisplayName(value));
}

template <typename T> bool _SetColorOptionVar(const MString& attrName, const T& values)
{
    bool rval = true;
    for (size_t i = 0; i < T::dimension; ++i) {
        rval = _SetOptionVar(_MangleColorAttribute(attrName, i), values[i]) && rval;
    }
    return rval;
}

template <typename T>
bool _GetAttribute(
    const MFnDependencyNode& node,
    const MString&           attrName,
    T&                       out,
    bool                     storeUserSetting)
{
    const auto plug = node.findPlug(attrName, true);
    if (plug.isNull()) {
        return false;
    }
    _GetFromPlug<T>(plug, out);
    if (storeUserSetting) {
        _SetOptionVar(attrName, out);
    }
    return true;
}

void _GetColorAttribute(
    const MFnDependencyNode&            node,
    const MString&                      attrName,
    GfVec3f&                            out,
    bool                                storeUserSetting,
    std::function<void(GfVec3f&, bool)> alphaOp = {})
{
    const auto plug = node.findPlug(attrName, true);
    if (plug.isNull()) {
        return;
    }

    out[0] = plug.child(0).asFloat();
    out[1] = plug.child(1).asFloat();
    out[2] = plug.child(2).asFloat();
    if (alphaOp) {
        // if alphaOp is provided, it is responsible for storing the settings
        alphaOp(out, storeUserSetting);
    } else {
        _SetColorOptionVar(attrName, out);
    }
}

void _GetColorAttribute(
    const MFnDependencyNode& node,
    const MString&           attrName,
    GfVec4f&                 out,
    bool                     storeUserSetting)
{
    GfVec3f color3;
    _GetColorAttribute(
        node, attrName, color3, storeUserSetting, [&](GfVec3f& color3, bool storeUserSetting) {
            const auto plugA = node.findPlug(_AlphaAttribute(attrName), true);
            if (plugA.isNull()) {
                TF_WARN("[mayaHydra] No Alpha plug for GfVec4f");
                return;
            }
            out[0] = color3[0];
            out[1] = color3[1];
            out[2] = color3[2];
            out[3] = plugA.asFloat();
            if (storeUserSetting) {
                _SetColorOptionVar(attrName, out);
            }
        });
}

bool _IsSupportedAttribute(const VtValue& v)
{
    return v.IsHolding<bool>() || v.IsHolding<int>() || v.IsHolding<unsigned int>()
        || v.IsHolding<float>() || v.IsHolding<GfVec3f>() || v.IsHolding<GfVec4f>()
        || v.IsHolding<TfToken>() || v.IsHolding<std::string>() || v.IsHolding<TfEnum>()
        || v.IsHolding<SdfAssetPath>();
}

TfToken _MangleString(
    const std::string& settingKey,
    const std::string& token,
    const std::string& replacement,
    std::string        str = {})
{
    std::size_t pos = 0;
    auto        delim = settingKey.find(token);
    while (delim != std::string::npos) {
        str += settingKey.substr(pos, delim - pos).c_str();
        str += replacement;
        pos = delim + token.size();
        delim = settingKey.find(token, pos);
    }
    str += settingKey.substr(pos, settingKey.size() - pos).c_str();
    return TfToken(str, TfToken::Immortal);
}

std::string _MangleRenderer(const TfToken& rendererName)
{
    return rendererName.IsEmpty() ? "" : (rendererName.GetString() + kMtohRendererPostFix);
}

TfToken _MangleString(const TfToken& settingKey, const TfToken& rendererName)
{
    return _MangleString(settingKey.GetString(), ":", kMtohNSToken, _MangleRenderer(rendererName));
}

TfToken _DeMangleString(const TfToken& settingKey, const TfToken& rendererName)
{
    assert(!rendererName.IsEmpty() && "No condition for this");
    return _MangleString(
        settingKey.GetString().substr(rendererName.size() + kMtohRendererPostFix.size()),
        kMtohNSToken,
        ":");
}

TfToken _MangleName(const TfToken& settingKey, const TfToken& rendererName = {})
{
    assert(
        rendererName.GetString().find(':') == std::string::npos
        && "Unexpected : token in plug-in name");
    return _MangleString(settingKey, rendererName);
}

// Escape a string for use inside a double-quoted MEL string literal. Double quotes become single
// quotes rather than \": attrControlGrp fails to create enum controls whose label holds a '"'.
std::string _EscapeMel(const std::string& str)
{
    std::string out;
    out.reserve(str.size());
    for (const char c : str) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += '\''; break;
        case '\n': out += "\\n"; break;
        default: out += c;
        }
    }
    return out;
}

std::string _QuoteMel(const std::string& str) { return "\"" + _EscapeMel(str) + "\""; }

// The delegate's display name without trailing periods, or the setting id if it has none.
std::string _DefaultSettingLabel(const HdRenderSettingDescriptor& desc)
{
    std::string label = desc.name;
    while (!label.empty() && (label.back() == '.' || label.back() == ' ')) {
        label.pop_back();
    }
    return label.empty() ? desc.key.GetString() : label;
}

} // namespace

MtohRenderGlobals::MtohRenderGlobals() { }

// Does the attribute in 'attrName' apply to the renderer
// XXX: Not the greatest check in the world, but currently no overlap in renderer-names
bool MtohRenderGlobals::AffectsRenderer(const TfToken& mangledAttr, const TfToken& rendererName)
{
    // If no explicit renderer, the setting affects them all
    if (rendererName.IsEmpty()) {
        return true;
    }
    return mangledAttr.GetString().find(_MangleRenderer(rendererName)) == 0;
}

bool MtohRenderGlobals::ApplySettings(
    HdRenderDelegate*    delegate,
    const TfToken&       rendererName,
    const TfTokenVector& attrNames) const
{
    const auto* settings = TfMapLookupPtr(_rendererSettings, rendererName);
    if (!settings) {
        return false;
    }

    bool appliedAny = false;
    if (!attrNames.empty()) {
        for (auto& mangledAttr : attrNames) {
            if (const auto* setting = TfMapLookupPtr(*settings, mangledAttr)) {
                delegate->SetRenderSetting(_DeMangleString(mangledAttr, rendererName), *setting);
                appliedAny = true;
            }
        }
    } else {
        for (auto&& setting : *settings) {
            delegate->SetRenderSetting(
                _DeMangleString(setting.first, rendererName), setting.second);
        }
        appliedAny = true;
    }

    return appliedAny;
}

void MtohRenderGlobals::OptionsPreamble()
{
    // The per-renderer Options procs are named after the render override, see BuildOptionsMenu.
    const auto preamble = TfStringReplace(
        _renderOverride_PreAmble, "{{overridePrefix}}", MayaHydra::MTOH_RENDER_OVERRIDE_PREFIX);
    MStatus status = MGlobal::executeCommand(preamble.c_str());
    if (status) {
        return;
    }
    TF_WARN("[mayaHydra] Error executing preamble:\n%s", preamble.c_str());
}

void MtohRenderGlobals::RegisterAttributeEditorCallback()
{
    // Not part of the preamble: the preamble runs once per process, but uninitializePlugin clears
    // every mayaHydra callback, so the hook must be added again on each plugin load.
    MGlobal::executeCommand(
        "catchQuiet(`callbacks -o mayaHydra -hook AETemplateCustomContent -removeCallback "
        "mtohRenderOverride_AEAttributesCallback`);\n"
        "callbacks -o mayaHydra -hook AETemplateCustomContent -addCallback "
        "mtohRenderOverride_AEAttributesCallback;");
}

void MtohRenderGlobals::BuildOptionsMenu(
    const MtohRendererDescription&       rendererDesc,
    const HdRenderSettingDescriptorList& rendererSettingDescriptors)
{
    // Every setting this renderer can show, in the delegate's order, then MayaHydra's own
    // per-renderer attributes. The layout file (see renderSettingsUiConfig.h) only places and
    // labels them: whether a setting exists is decided here.
    struct UiEntry
    {
        TfToken     key;
        std::string defaultLabel;
        std::string delegateName; // The delegate's display name, empty for MayaHydra attributes.
        std::string annotation;   // Fixed tooltip, used when delegateName is empty.
        std::string attrName;
    };
    std::vector<UiEntry> entries;
    for (const auto& desc : rendererSettingDescriptors) {
        if (!_IsSupportedAttribute(desc.defaultValue)) {
            continue;
        }
        entries.push_back({ desc.key,
                            _DefaultSettingLabel(desc),
                            desc.name,
                            {},
                            _MangleName(desc.key, rendererDesc.rendererName).GetString() });
    }
    const auto addMayaHydraAttr
        = [&](const TfToken& attr, const char* label, const char* annotation) {
              entries.push_back({ attr, label, {}, annotation, _MangleName(attr).GetString() });
          };

    if (rendererDesc.rendererName == MayaHydraRendererTokens->HdStormRendererPlugin) {
        addMayaHydraAttr(
            MtohTokens->mtohMaximumShadowMapResolution,
            "Max shadow map size",
            "Maximum shadow map size");
    }
    // The renderers that support the outline (see MtohRenderOverride::_RendererSupportsOutline()).
    if (rendererDesc.rendererName == MayaHydraRendererTokens->HdStormRendererPlugin
        || rendererDesc.rendererName == MayaHydraRendererTokens->HdFlashRendererPlugin) {
        // Dropdown: Outline Selection / Legacy Selection.
        addMayaHydraAttr(
            MtohTokens->mayaHydraSelectionHighlightMode,
            "Selection highlight",
            "Selection highlight mode (Outline or Legacy)");
    }
    if (rendererDesc.rendererName == MayaHydraRendererTokens->HdStormRendererPlugin) {
        // Checkbox, shown only where hover can work: it needs Qt for the cursor position
        // (MAYAHYDRA_HAS_QT) and the Outline mode. CreateAttributes() still creates the attribute
        // everywhere, so scenes saved on a capable build load on any build.
#if defined(MAYAHYDRA_HAS_QT) && !defined(MAYAHYDRA_NO_OUTLINE_SELECTION_HIGHLIGHT)
        addMayaHydraAttr(
            MtohTokens->mayaHydraOutlineHoverHighlighting,
            "Hover highlight (experimental)",
            "Outline the object under the cursor. Requires the Outline selection highlight mode.");
#endif
    }
    // Refine level and the display purpose attributes are shared by all renderers: see
    // mtohRenderOverride_GlobalOptions in the preamble.

    const auto renderer = _QuoteMel(rendererDesc.rendererName.GetString());

    std::stringstream ss;
    // The "All renderers" group shares the option box, so its labels count too.
    size_t     longestLabel = std::string("Render purpose").size();
    const auto emitEntry = [&](const UiEntry& entry, const std::string& labelOverride) {
        const std::string& label = labelOverride.empty() ? entry.defaultLabel : labelOverride;
        longestLabel = std::max(longestLabel, label.size());
        // Tooltip: the delegate's full display name, then the setting id it maps to.
        std::string annotation = !entry.annotation.empty() ? entry.annotation
            : entry.delegateName.empty() || entry.delegateName == label
            ? entry.key.GetString()
            : entry.delegateName + " (" + entry.key.GetString() + ")";
        ss << "\t\tmtohRenderOverride_AddAttribute(" << renderer << ',' << _QuoteMel(annotation)
           << ',' << _QuoteMel(entry.attrName) << ',' << _QuoteMel(label) << ", $fromAE);\n";
    };
    const auto emitGroup = [&](const RenderSettingsUiCategory&                   category,
                               const std::vector<std::pair<const UiEntry*, std::string>>& items) {
        if (items.empty()) {
            return;
        }
        ss << "\tmtohRenderOverride_BeginGroup(" << _QuoteMel(category.label) << ", "
           << (category.collapsed ? 1 : 0) << ", $fromAE);\n";
        for (const auto& item : items) {
            emitEntry(*item.first, item.second);
        }
        ss << "\tmtohRenderOverride_EndGroup($fromAE);\n";
    };

    ss << "global proc " << rendererDesc.overrideName << "Options(int $fromAE) {\n";
    if (const auto* layout = MtohGetRenderSettingsUiLayout(rendererDesc.rendererName)) {
        std::unordered_map<TfToken, const UiEntry*, TfToken::HashFunctor> byKey;
        for (const auto& entry : entries) {
            byKey.emplace(entry.key, &entry);
        }
        // A key is shown once: the first category that lists it wins, hidden keys never show.
        TfToken::Set placed = layout->hidden;

        std::vector<std::vector<std::pair<const UiEntry*, std::string>>> groups(
            layout->categories.size());
        for (size_t i = 0; i < layout->categories.size(); ++i) {
            for (const auto& setting : layout->categories[i].settings) {
                // Keys this renderer does not report are skipped: one file can serve several
                // delegate versions.
                const auto it = byKey.find(setting.key);
                if (it != byKey.end() && placed.insert(setting.key).second) {
                    groups[i].emplace_back(it->second, setting.label);
                }
            }
        }
        // Settings no category lists go to the "other" group, in the delegate's order. If a
        // category has the same label, they are appended to it rather than shown twice.
        std::vector<std::pair<const UiEntry*, std::string>>* leftovers = nullptr;
        for (size_t i = 0; i < layout->categories.size() && !leftovers; ++i) {
            if (layout->categories[i].label == layout->other.label) {
                leftovers = &groups[i];
            }
        }
        std::vector<std::pair<const UiEntry*, std::string>> otherGroup;
        if (!leftovers) {
            leftovers = &otherGroup;
        }
        for (const auto& entry : entries) {
            if (placed.insert(entry.key).second) {
                leftovers->emplace_back(&entry, std::string());
            }
        }
        for (size_t i = 0; i < layout->categories.size(); ++i) {
            emitGroup(layout->categories[i], groups[i]);
        }
        emitGroup(layout->other, otherGroup);
    } else {
        // No layout file for this renderer: a flat list in the delegate's order.
        for (const auto& entry : entries) {
            emitEntry(entry, std::string());
        }
    }
    ss << "}\n";

    const auto optionsCommand = ss.str();
    auto status = MGlobal::executeCommand(optionsCommand.c_str());
    if (!status) {
        TF_WARN(
            "[mayaHydra] Error in render delegate options function: \n%s",
            status.errorString().asChar());
    }

    // Size the option box's label column to the longest label: an estimate of the label font's
    // average character width, kept within limits so one very long label does not make the window
    // huge (the full text stays in the tooltip). The controls keep the width they had before.
    constexpr int kPixelsPerChar = 7;
    constexpr int kLabelPadding = 16;
    constexpr int kMinLabelWidth = 150;
    constexpr int kMaxLabelWidth = 400;
    constexpr int kControlsWidth = 260;
    const int     labelWidth = std::clamp(
        static_cast<int>(longestLabel) * kPixelsPerChar + kLabelPadding,
        kMinLabelWidth,
        kMaxLabelWidth);

    auto optionBoxCommand = TfStringReplace(
        _renderOverrideOptionBoxTemplate, "{{override}}", rendererDesc.overrideName.GetText());
    optionBoxCommand
        = TfStringReplace(optionBoxCommand, "{{hydraplugin}}", rendererDesc.rendererName);
    optionBoxCommand
        = TfStringReplace(optionBoxCommand, "{{hydraDisplayName}}", rendererDesc.displayName);
    optionBoxCommand = TfStringReplace(
        optionBoxCommand,
        "{{hydraRenderDelegateName}}",
        rendererDesc.rendererName); // Is the name of the render delegate since we are displaying
                                    // its settings
    optionBoxCommand
        = TfStringReplace(optionBoxCommand, "{{labelWidth}}", std::to_string(labelWidth));
    optionBoxCommand = TfStringReplace(
        optionBoxCommand, "{{windowWidth}}", std::to_string(labelWidth + kControlsWidth));

    status = MGlobal::executeCommand(optionBoxCommand.c_str());
    if (!status) {
        TF_WARN(
            "[mayaHydra] Error in render override option box command function: \n%s",
            status.errorString().asChar());
    }
}

class MtohRenderGlobals::MtohSettingFilter
{
    TfToken       _attrName;
    MString       _mayaString;
    const TfToken _inFilter;
    bool          _isAttributeFilt;

public:
    MtohSettingFilter(const GlobalParams& params)
        : _inFilter(params.filter)
        , _isAttributeFilt(!(params.filterIsRenderer || _inFilter.IsEmpty()))
    {
    }

    // Create the mangled key, and convert it to a Maya string if needed
    bool operator()(const TfToken& attr, const TfToken& renderer = {})
    {
        _attrName = _MangleName(attr, renderer);
        if (attributeFilter()) {
            if (_inFilter != _attrName) {
                return false;
            }
        } else if (renderFilter()) {
            // Allow everything for all renderers through
            if (!renderer.IsEmpty() && renderer != _inFilter) {
                return false;
            }
        }
        _mayaString = MString(_attrName.GetText());
        return true;
    }
    bool attributeFilter() const { return _isAttributeFilt; }
    bool renderFilter() const { return !_isAttributeFilt && !_inFilter.IsEmpty(); }
    bool affectsRenderer(const TfToken& renderer)
    {
        // If there's no filter, then the filter DOES affect this renderer
        if (_inFilter.IsEmpty()) {
            return true;
        }
        // If it's an attribute-filter, test with mangling, otherwise the renderer-names should
        // match
        return _isAttributeFilt ? AffectsRenderer(_inFilter, renderer) : renderer == _inFilter;
    }
    const TfToken& attrName() const { return _attrName; }
    const MString& mayaString() const { return _mayaString; }
};

MObject MtohRenderGlobals::CreateAttributes(const GlobalParams& params)
{
    MSelectionList slist;
    slist.add(_tokens->defaultRenderGlobals.GetText());

    MObject mayaObject;
    if (slist.length() == 0 || !slist.getDependNode(0, mayaObject)) {
        return mayaObject;
    }

    MStatus           status;
    MFnDependencyNode node(mayaObject, &status);
    if (!status) {
        return MObject();
    }

    // This static is used to setup the default values
    //
    static const MtohRenderGlobals defGlobals;

    MtohSettingFilter filter(params);
    const bool        userDefaults = params.fallbackToUserDefaults;

    if (filter(_tokens->mtohMotionSampleStart)) {
        _CreateFloatAttribute(
            node, filter.mayaString(), defGlobals.delegateParams.motionSampleStart, userDefaults);
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    if (filter(_tokens->mtohMotionSampleEnd)) {
        _CreateFloatAttribute(
            node, filter.mayaString(), defGlobals.delegateParams.motionSampleEnd, userDefaults);
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    
    if (filter(_tokens->mayaHydraRenderPurpose)) {
        _CreateBoolAttribute(
            node,
            filter.mayaString(),
            defGlobals.delegateParams.renderPurpose,
            userDefaults);
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    if (filter(_tokens->mayaHydraProxyPurpose)) {
        _CreateBoolAttribute(
            node,
            filter.mayaString(),
            defGlobals.delegateParams.proxyPurpose,
            userDefaults);
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    if (filter(_tokens->mayaHydraGuidePurpose)) {
        _CreateBoolAttribute(
            node,
            filter.mayaString(),
            defGlobals.delegateParams.guidePurpose,
            userDefaults);
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    if (filter(_tokens->mtohTextureMemoryPerTexture)) {
        _CreateIntAttribute(
            node,
            filter.mayaString(),
            defGlobals.delegateParams.textureMemoryPerTexture / 1024,
            userDefaults,
            [](MFnNumericAttribute& nAttr) {
                nAttr.setMin(1);
                nAttr.setMax(256 * 1024);
                nAttr.setSoftMin(1 * 1024);
                nAttr.setSoftMin(16 * 1024);
            });
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    if (filter(MtohTokens->mtohMaximumShadowMapResolution)) {
        _CreateIntAttribute(
            node,
            filter.mayaString(),
            defGlobals.delegateParams.maximumShadowMapResolution,
            userDefaults,
            [](MFnNumericAttribute& nAttr) {
                nAttr.setMin(32);
                nAttr.setMax(8192);
            });
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    if (filter(MtohTokens->mayaHydraRefinementLevel)) {
        _CreateIntAttribute(
            node,
            filter.mayaString(),
            defGlobals.delegateParams.refineLevel,
            userDefaults,
            [](MFnNumericAttribute& nAttr) {
                nAttr.setMin(0);
                nAttr.setMax(8);
            });
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    if (filter(MtohTokens->mayaHydraSelectionHighlightMode)) {
#ifdef MAYAHYDRA_NO_OUTLINE_SELECTION_HIGHLIGHT
        static const TfTokenVector kSelectionHighlightModes = { TfToken("Legacy Selection") };
#else
        static const TfTokenVector kSelectionHighlightModes
            = { TfToken("Outline Selection"), TfToken("Legacy Selection") };
#endif
        _CreateEnumAttribute(
            node,
            filter.mayaString(),
            kSelectionHighlightModes,
            kSelectionHighlightModes[0], // Outline, or Legacy where Outline is unavailable
            userDefaults);
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    // Script-only profiling switch, deliberately absent from BuildOptionsMenu: runs the hover hit
    // test without drawing the hover, so pick cost can be measured apart from draw cost.
    // Session-only: never restored from user defaults.
    if (filter(MtohTokens->mayaHydraForceEnableInteractiveHitTest)) {
        _CreateSessionOnlyBoolAttribute(
            node, filter.mayaString(), defGlobals.forceEnableInteractiveHitTest);
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    if (filter(MtohTokens->mayaHydraOutlineHoverHighlighting)) {
        _CreateBoolAttribute(
            node,
            filter.mayaString(),
            defGlobals.outlineHoverHighlighting,
            userDefaults);
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    // Script-only: deliberately absent from BuildOptionsMenu. Session-only: never restored from
    // user defaults.
    if (filter(MtohTokens->mayaHydraForceDisableSelectionHighlight)) {
        _CreateSessionOnlyBoolAttribute(
            node, filter.mayaString(), defGlobals.forceDisableSelectionHighlight);
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }
    // Script-only: deliberately absent from BuildOptionsMenu. Session-only: never restored from
    // user defaults.
    if (filter(MtohTokens->mayaHydraEnableDefaultOutlines)) {
        _CreateSessionOnlyBoolAttribute(
            node, filter.mayaString(), defGlobals.enableDefaultOutlines);
        if (filter.attributeFilter()) {
            return mayaObject;
        }
    }

    for (const auto& rit : MtohGetRendererSettings()) {
        const auto rendererName = rit.first;
        // Skip over all the settings for this renderer if it doesn't match
        if (!filter.affectsRenderer(rendererName)) {
            continue;
        }

        for (const auto& attr : rit.second) {
            if (!filter(attr.key, rendererName)) {
                continue;
            }

            if (attr.defaultValue.IsHolding<bool>()) {
                _CreateBoolAttribute(
                    node,
                    filter.mayaString(),
                    attr.defaultValue.UncheckedGet<bool>(),
                    userDefaults);
            } else if (attr.defaultValue.IsHolding<int>()) {
                _CreateIntAttribute(
                    node, filter.mayaString(), attr.defaultValue.UncheckedGet<int>(), userDefaults);
            } else if (attr.defaultValue.IsHolding<unsigned int>()) {
                _CreateIntAttribute(
                    node,
                    filter.mayaString(),
                    static_cast<int>(attr.defaultValue.UncheckedGet<unsigned int>()),
                    userDefaults);
            } else if (attr.defaultValue.IsHolding<float>()) {
                _CreateFloatAttribute(
                    node,
                    filter.mayaString(),
                    attr.defaultValue.UncheckedGet<float>(),
                    userDefaults);
            } else if (attr.defaultValue.IsHolding<GfVec3f>()) {
                _CreateColorAttribute(
                    node,
                    filter.mayaString(),
                    attr.defaultValue.UncheckedGet<GfVec3f>(),
                    userDefaults);
            } else if (attr.defaultValue.IsHolding<GfVec4f>()) {
                _CreateColorAttribute(
                    node,
                    filter.mayaString(),
                    attr.defaultValue.UncheckedGet<GfVec4f>(),
                    userDefaults);
            } else if (attr.defaultValue.IsHolding<TfToken>()) {
                // If this attribute type has AllowedTokens set, we treat it as an enum instead, so
                // that only valid values are available.```
                bool createdAsEnum = false;
                if (auto primDef = UsdRenderSettings().GetSchemaClassPrimDefinition()) {
                    VtTokenArray allowedTokens;

                    if (primDef->GetPropertyMetadata(
                            TfToken(attr.key), SdfFieldKeys->AllowedTokens, &allowedTokens)) {
                        // Generate dropdown from allowedTokens
                        TfTokenVector tokens(allowedTokens.begin(), allowedTokens.end());
                        _CreateEnumAttribute(
                            node,
                            filter.mayaString(),
                            tokens,
                            attr.defaultValue.UncheckedGet<TfToken>(),
                            userDefaults);
                        createdAsEnum = true;
                    }
                }

                if (!createdAsEnum) {
                    _CreateStringAttribute(
                        node,
                        filter.mayaString(),
                        attr.defaultValue.UncheckedGet<TfToken>().GetString(),
                        userDefaults);
                }
            } else if (attr.defaultValue.IsHolding<std::string>()) {
                _CreateStringAttribute(
                    node,
                    filter.mayaString(),
                    attr.defaultValue.UncheckedGet<std::string>(),
                    userDefaults);
            } else if (attr.defaultValue.IsHolding<SdfAssetPath>()) {
                _CreateStringAttribute(
                    node,
                    filter.mayaString(),
                    attr.defaultValue.UncheckedGet<SdfAssetPath>().GetAssetPath(),
                    userDefaults,
                    true);
            } else if (attr.defaultValue.IsHolding<TfEnum>()) {
                _CreateEnumAttribute(
                    node,
                    filter.mayaString(),
                    attr.defaultValue.UncheckedGet<TfEnum>(),
                    userDefaults);
            } else {
                assert(
                    !_IsSupportedAttribute(attr.defaultValue)
                    && "_IsSupportedAttribute out of synch");

                TF_WARN(
                    "[mayaHydra] Ignoring setting: '%s' for %s",
                    attr.key.GetText(),
                    rendererName.GetText());
            }
            if (filter.attributeFilter()) {
                break;
            }
        }
    }
    return mayaObject;
}

const MtohRenderGlobals&
MtohRenderGlobals::GetInstance(const GlobalParams& params, bool storeUserSetting)
{
    static MtohRenderGlobals globals;
    const auto               obj = CreateAttributes(params);
    if (obj.isNull()) {
        return globals;
    }

    MStatus           status;
    MFnDependencyNode node(obj, &status);
    if (!status) {
        return globals;
    }

    MtohSettingFilter filter(params);
    
    if (filter(_tokens->mayaHydraProxyPurpose)
        && _GetAttribute(
            node,
            filter.mayaString(),
            globals.delegateParams.proxyPurpose,
            storeUserSetting)) {
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    if (filter(_tokens->mayaHydraRenderPurpose)
        && _GetAttribute(
            node,
            filter.mayaString(),
            globals.delegateParams.renderPurpose,
            storeUserSetting)) {
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    if (filter(_tokens->mayaHydraGuidePurpose)
        && _GetAttribute(
            node,
            filter.mayaString(),
            globals.delegateParams.guidePurpose,
            storeUserSetting)) {
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    if (filter(_tokens->mtohTextureMemoryPerTexture)
        && _GetAttribute(
            node,
            filter.mayaString(),
            globals.delegateParams.textureMemoryPerTexture,
            storeUserSetting)) {
        globals.delegateParams.textureMemoryPerTexture *= 1024;
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    if (filter(_tokens->mtohMotionSampleStart)) {
        _GetAttribute(
            node, filter.mayaString(), globals.delegateParams.motionSampleStart, storeUserSetting);
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    if (filter(_tokens->mtohMotionSampleEnd)) {
        _GetAttribute(
            node, filter.mayaString(), globals.delegateParams.motionSampleEnd, storeUserSetting);
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    if (filter(MtohTokens->mtohMaximumShadowMapResolution)) {
        _GetAttribute(
            node,
            filter.mayaString(),
            globals.delegateParams.maximumShadowMapResolution,
            storeUserSetting);
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    if (filter(MtohTokens->mayaHydraRefinementLevel)) {
        _GetAttribute(
            node,
            filter.mayaString(),
            globals.delegateParams.refineLevel,
            storeUserSetting);
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    if (filter(MtohTokens->mayaHydraSelectionHighlightMode)) {
#ifdef MAYAHYDRA_NO_OUTLINE_SELECTION_HIGHLIGHT
        TfToken mode("Legacy Selection");
#else
        TfToken mode("Outline Selection");
#endif
        _GetAttribute(
            node,
            filter.mayaString(),
            mode,
            storeUserSetting);
        globals.outlineSelectionHighlight = (mode == TfToken("Outline Selection"));
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    if (filter(MtohTokens->mayaHydraOutlineHoverHighlighting)) {
        _GetAttribute(
            node,
            filter.mayaString(),
            globals.outlineHoverHighlighting,
            storeUserSetting);
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    // Session-only (see _CreateSessionOnlyBoolAttribute): never stored as a user setting.
    if (filter(MtohTokens->mayaHydraEnableDefaultOutlines)) {
        _GetAttribute(
            node,
            filter.mayaString(),
            globals.enableDefaultOutlines,
            false);
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    // Session-only (see _CreateSessionOnlyBoolAttribute): never stored as a user setting.
    if (filter(MtohTokens->mayaHydraForceEnableInteractiveHitTest)) {
        _GetAttribute(
            node,
            filter.mayaString(),
            globals.forceEnableInteractiveHitTest,
            false);
        if (filter.attributeFilter()) {
            return globals;
        }
    }
    // Session-only (see _CreateSessionOnlyBoolAttribute): never stored as a user setting.
    if (filter(MtohTokens->mayaHydraForceDisableSelectionHighlight)) {
        _GetAttribute(
            node,
            filter.mayaString(),
            globals.forceDisableSelectionHighlight,
            false);
        if (filter.attributeFilter()) {
            return globals;
        }
    }

    for (const auto& rit : MtohGetRendererSettings()) {
        const auto rendererName = rit.first;
        // Skip over all the settings for this renderer if it doesn't match
        if (!filter.affectsRenderer(rendererName)) {
            continue;
        }

        auto& settings = globals._rendererSettings[rendererName];
        settings.reserve(rit.second.size());
        for (const auto& attr : rit.second) {
            if (!filter(attr.key, rendererName)) {
                continue;
            }

            if (attr.defaultValue.IsHolding<bool>()) {
                auto v = attr.defaultValue.UncheckedGet<bool>();
                _GetAttribute(node, filter.mayaString(), v, storeUserSetting);
                settings[filter.attrName()] = v;
            } else if (attr.defaultValue.IsHolding<int>()) {
                auto v = attr.defaultValue.UncheckedGet<int>();
                _GetAttribute(node, filter.mayaString(), v, storeUserSetting);
                settings[filter.attrName()] = v;
            } else if (attr.defaultValue.IsHolding<unsigned int>()) {
                auto v = attr.defaultValue.UncheckedGet<unsigned int>();
                _GetAttribute(node, filter.mayaString(), v, storeUserSetting);
                settings[filter.attrName()] = v;
            } else if (attr.defaultValue.IsHolding<float>()) {
                auto v = attr.defaultValue.UncheckedGet<float>();
                _GetAttribute(node, filter.mayaString(), v, storeUserSetting);
                settings[filter.attrName()] = v;
            } else if (attr.defaultValue.IsHolding<GfVec3f>()) {
                auto v = attr.defaultValue.UncheckedGet<GfVec3f>();
                _GetColorAttribute(node, filter.mayaString(), v, storeUserSetting);
                settings[filter.attrName()] = v;
            } else if (attr.defaultValue.IsHolding<GfVec4f>()) {
                auto v = attr.defaultValue.UncheckedGet<GfVec4f>();
                _GetColorAttribute(node, filter.mayaString(), v, storeUserSetting);
                settings[filter.attrName()] = v;
            } else if (attr.defaultValue.IsHolding<TfToken>()) {
                auto v = attr.defaultValue.UncheckedGet<TfToken>();
                _GetAttribute(node, filter.mayaString(), v, storeUserSetting);
                settings[filter.attrName()] = v;
            } else if (attr.defaultValue.IsHolding<std::string>()) {
                auto v = attr.defaultValue.UncheckedGet<std::string>();
                _GetAttribute(node, filter.mayaString(), v, storeUserSetting);
                settings[filter.attrName()] = v;
            } else if (attr.defaultValue.IsHolding<TfEnum>()) {
                auto v = attr.defaultValue.UncheckedGet<TfEnum>();
                _GetAttribute(node, filter.mayaString(), v, storeUserSetting);
                settings[filter.attrName()] = v;
            } else if (attr.defaultValue.IsHolding<SdfAssetPath>()) {
                auto v = attr.defaultValue.UncheckedGet<SdfAssetPath>();
                _GetAttribute(node, filter.mayaString(), v, storeUserSetting);
                settings[filter.attrName()] = v;
            } else {
                assert(
                    !_IsSupportedAttribute(attr.defaultValue)
                    && "_IsSupportedAttribute out of synch");

                TF_WARN(
                    "[mayaHydra] Can't get setting: '%s' for %s",
                    attr.key.GetText(),
                    rendererName.GetText());
            }
            if (filter.attributeFilter()) {
                break;
            }
        }
    }
    return globals;
}

const MtohRenderGlobals& MtohRenderGlobals::GetInstance(bool storeUserSetting)
{
    return GetInstance(GlobalParams(), storeUserSetting);
}

const MtohRenderGlobals&
MtohRenderGlobals::GlobalChanged(const GlobalParams& params, bool storeUserSetting)
{
    return GetInstance(params, storeUserSetting);
}

PXR_NAMESPACE_CLOSE_SCOPE

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {
namespace RenderGlobalsUtils {

TfToken GetPurposeRenderTagFromAttrName(const TfToken& attrName)
{
    static const std::map<TfToken, TfToken> attrToTag {
        { TfToken("mayaHydraRenderPurpose"),    HdRenderTagTokens->render },
        { TfToken("mayaHydraProxyPurpose"),     HdRenderTagTokens->proxy },
        { TfToken("mayaHydraGuidePurpose"),     HdRenderTagTokens->guide }
    };
    auto found = attrToTag.find(attrName);
    if (found != attrToTag.end()) {
        return found->second;
    }
    TF_CODING_ERROR("Unknown purpose attribute name '%s'", attrName.GetText());
    return {};
}

// Read the included purposes from the MayaHydra render globals.
std::set<PXR_NS::TfToken> GetIncludedPurposes()
{
    static MObject rgObj;
    MStatus status;

    if (rgObj.isNull()) {
        MSelectionList sn;
        status = sn.add(MString(_tokens->defaultRenderGlobals.GetText()));
        TF_AXIOM(status == MS::kSuccess);
        status = sn.getDependNode(0, rgObj);
        TF_AXIOM(status == MS::kSuccess);
    }

    std::set<PXR_NS::TfToken> includedPurposes;
    MFnDependencyNode rgFn(rgObj);
    constexpr bool setOptionVar = false;
    for (auto attrName : {_tokens->mayaHydraProxyPurpose, _tokens->mayaHydraRenderPurpose, _tokens->mayaHydraGuidePurpose}) {
        bool enabled;
        // Purpose attributes are dynamically created on the
        // defaultRenderGlobals node, so incorrectly querying them before
        // creation will fail.
        if (!_GetAttribute(
                rgFn, MString(attrName.GetText()), enabled, setOptionVar)) {
            TF_WARN("_GetAttribute(%s.%s) failed.", rgFn.name().asChar(), attrName.GetText());
            continue;
        }
        if (enabled) {
            includedPurposes.insert(GetPurposeRenderTagFromAttrName(attrName));
        }
    }

    return includedPurposes;
}

}
}
