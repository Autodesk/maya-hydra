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
#ifndef MAYAHYDRALIB_CAMERA_EXPOSURE_H
#define MAYAHYDRALIB_CAMERA_EXPOSURE_H

// ---------------------------------------------------------------------------
// Physical-camera exposure attributes (auto-added to the Maya camera shape).
//
// The attribute names, value tables, enum field order and the EV formula are
// defined once here so that every scene-index path which translates a Maya
// camera shares them, and a camera reports the same exposure no matter which
// path produced its prim.
//
// f-stop / shutter / ISO are full-stop drop-downs (MFnEnumAttribute) with a
// trailing "Custom" field; when "Custom" is selected the companion float is
// read instead. The enum field order MUST match the corresponding value table
// below; the trailing "Custom" field has index == table size.
//
// Everything is header-inline: the functions are `inline` and the name
// constants and tables are `inline constexpr`, so each is a single entity
// across translation units (ODR-safe).
// ---------------------------------------------------------------------------

#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>

#include <cmath>

namespace MayaHydraCameraExposure {

// Long names stay unique for setAttr / scripting; short names are the DG
// short-name aliases.
inline constexpr const char* kAttrUsePhysicalCamera      = "mayaHydraUsePhysicalCamera";
inline constexpr const char* kAttrUsePhysicalCameraShort = "mhUsePhysCam";
inline constexpr const char* kAttrFStopPreset            = "mayaHydraExposureFStopPreset";
inline constexpr const char* kAttrFStopPresetShort       = "mhExpFStopP";
inline constexpr const char* kAttrFStopCustom            = "mayaHydraExposureFStopCustom";
inline constexpr const char* kAttrFStopCustomShort       = "mhExpFStopC";
inline constexpr const char* kAttrShutterPreset          = "mayaHydraExposureShutterPreset";
inline constexpr const char* kAttrShutterPresetShort     = "mhExpShutP";
inline constexpr const char* kAttrShutterCustom          = "mayaHydraExposureShutterCustom";
inline constexpr const char* kAttrShutterCustomShort     = "mhExpShutC";
inline constexpr const char* kAttrISOPreset              = "mayaHydraExposureISOPreset";
inline constexpr const char* kAttrISOPresetShort         = "mhExpISOP";
inline constexpr const char* kAttrISOCustom              = "mayaHydraExposureISOCustom";
inline constexpr const char* kAttrISOCustomShort         = "mhExpISOC";
inline constexpr const char* kAttrEVComp                 = "mayaHydraExposureEVComp";
inline constexpr const char* kAttrEVCompShort            = "mhExpEVComp";

// UI labels shown in the Channel Box / Attribute Editor (nice-name override).
inline constexpr const char* kUiUsePhysicalCamera = "Use Physical Exposure";
inline constexpr const char* kUiFStopPreset       = "Exposure F-Stop";
inline constexpr const char* kUiFStopCustom       = "Exposure F-Stop (Custom)";
inline constexpr const char* kUiShutterPreset     = "Exposure Shutter Speed";
inline constexpr const char* kUiShutterCustom     = "Exposure Shutter (Custom, s)";
inline constexpr const char* kUiISOPreset         = "Exposure ISO";
inline constexpr const char* kUiISOCustom         = "Exposure ISO (Custom)";
inline constexpr const char* kUiEVComp            = "Exposure Compensation (EV)";

inline constexpr const char* const kFStopLabels[]
    = { "f/1.0", "f/1.4", "f/2.0", "f/2.8", "f/4.0", "f/5.6", "f/8.0", "f/11", "f/16", "f/22" };
inline constexpr float kFStopValues[]
    = { 1.0f, 1.4f, 2.0f, 2.8f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f, 22.0f };
inline constexpr short kFStopCount   = 10;
inline constexpr short kFStopDefault = 4; // f/4

inline constexpr const char* const kShutterLabels[]
    = { "1", "1/2", "1/4", "1/8", "1/15", "1/30", "1/60", "1/125", "1/250", "1/500", "1/1000" };
inline constexpr float kShutterValues[] = { 1.0f, 1.0f / 2, 1.0f / 4, 1.0f / 8, 1.0f / 15,
    1.0f / 30, 1.0f / 60, 1.0f / 125, 1.0f / 250, 1.0f / 500, 1.0f / 1000 };
inline constexpr short kShutterCount   = 11;
inline constexpr short kShutterDefault = 7; // 1/125

inline constexpr const char* const kISOLabels[]
    = { "100", "200", "400", "800", "1600", "3200", "6400" };
inline constexpr float kISOValues[] = { 100.0f, 200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f, 6400.0f };
inline constexpr short kISOCount   = 7;
inline constexpr short kISODefault = 2; // 400

// Convert physical photographic exposure parameters to a single linear scene
// scale using the standard photographic formula:
//   EV100        = log2( N^2 / t ) - log2( S / 100 )
//   linear scale = 2^( -EV100 + evComp )
// Returns 1.0 when any input is non-positive (safe pass-through default).
inline float ComputeLinearExposureScale(float fStop, float shutterTime, float iso, float evComp)
{
    if (!(fStop > 0.0f) || !(shutterTime > 0.0f) || !(iso > 0.0f)) {
        return 1.0f;
    }
    const float ev100 = std::log2((fStop * fStop) / shutterTime) - std::log2(iso / 100.0f);
    return std::exp2(-ev100 + evComp);
}

// Resolve one exposure control: read the enum preset index; if it selects a
// real stop return the value from the table, otherwise ("Custom" == count, or
// out of range) read the companion float attribute.
inline float ResolveStop(
    MFnDependencyNode& dep,
    const char*        presetAttr,
    const char*        customAttr,
    const float*       table,
    short              count,
    short              defaultIdx)
{
    MStatus     status;
    MPlug       presetPlug = dep.findPlug(presetAttr, false, &status);
    const short idx = (!status.error() && !presetPlug.isNull()) ? presetPlug.asShort() : defaultIdx;
    if (idx >= 0 && idx < count) {
        return table[idx];
    }
    MPlug customPlug = dep.findPlug(customAttr, false, &status);
    if (!status.error() && !customPlug.isNull()) {
        return customPlug.asFloat();
    }
    return table[defaultIdx];
}

// Compute the camera's linearExposureScale from its physical-camera attributes.
// When usePhysicalCamera is false (or the attributes are absent) returns 1.0 so
// scenes that do not opt in to physical units render unchanged.
inline float ComputeCameraLinearExposureScale(const MDagPath& dag)
{
    MStatus           status;
    MFnDependencyNode dep(dag.node(), &status);
    if (status.error()) {
        return 1.0f;
    }
    MPlug usePhysPlug = dep.findPlug(kAttrUsePhysicalCamera, false, &status);
    if (status.error() || usePhysPlug.isNull() || !usePhysPlug.asBool()) {
        return 1.0f;
    }
    const float fStop = ResolveStop(
        dep, kAttrFStopPreset, kAttrFStopCustom, kFStopValues, kFStopCount, kFStopDefault);
    const float shutter = ResolveStop(
        dep, kAttrShutterPreset, kAttrShutterCustom, kShutterValues, kShutterCount,
        kShutterDefault);
    const float iso
        = ResolveStop(dep, kAttrISOPreset, kAttrISOCustom, kISOValues, kISOCount, kISODefault);

    float evComp = 0.0f;
    MPlug evPlug = dep.findPlug(kAttrEVComp, false, &status);
    if (!status.error() && !evPlug.isNull()) {
        evComp = evPlug.asFloat();
    }
    return ComputeLinearExposureScale(fStop, shutter, iso, evComp);
}

inline void EnsureEnumAttr(
    MFnDependencyNode&  dep,
    const char*         lng,
    const char*         shrt,
    const char*         uiName,
    const char* const*  labels,
    short               count,
    short               defaultIdx)
{
    MStatus status;
    if (!dep.attribute(lng, &status).isNull()) {
        return; // Already present.
    }
    MFnEnumAttribute fn;
    MObject          attr = fn.create(lng, shrt, defaultIdx, &status);
    if (status.error()) {
        return;
    }
    for (short i = 0; i < count; ++i) {
        fn.addField(labels[i], i);
    }
    fn.addField("Custom", count);
    fn.setStorable(true);
    fn.setKeyable(true);
    // The long name stays unique for setAttr / scripting, so the friendly label
    // is applied as a nice-name override.
    fn.setNiceNameOverride(uiName);
    dep.addAttribute(attr);
}

inline void EnsureFloatAttr(
    MFnDependencyNode& dep,
    const char*        lng,
    const char*        shrt,
    const char*        uiName,
    float              def)
{
    MStatus status;
    if (!dep.attribute(lng, &status).isNull()) {
        return;
    }
    MFnNumericAttribute fn;
    MObject             attr = fn.create(lng, shrt, MFnNumericData::kFloat, def, &status);
    if (status.error()) {
        return;
    }
    fn.setStorable(true);
    fn.setKeyable(true);
    fn.setNiceNameOverride(uiName);
    dep.addAttribute(attr);
}

inline void EnsureBoolAttr(
    MFnDependencyNode& dep,
    const char*        lng,
    const char*        shrt,
    const char*        uiName,
    bool               def)
{
    MStatus status;
    if (!dep.attribute(lng, &status).isNull()) {
        return;
    }
    MFnNumericAttribute fn;
    MObject attr = fn.create(lng, shrt, MFnNumericData::kBoolean, def ? 1.0 : 0.0, &status);
    if (status.error()) {
        return;
    }
    fn.setStorable(true);
    fn.setKeyable(true);
    fn.setNiceNameOverride(uiName);
    dep.addAttribute(attr);
}

// Add the physical-camera exposure attributes to the camera shape if they are
// missing. Each add is guarded by a presence check so this is idempotent and
// does its work at most once per camera. Referenced / locked nodes cannot take
// plain dynamic attributes, so they are skipped; the reader then simply reports
// a 1.0 (pass-through) exposure scale.
//
// NOTE: This authors DG attributes, so it must not run during Hydra evaluation
// or draw. Call it from adapter setup, never from the per-frame data-source
// pull.
inline void EnsureExposureAttributes(const MObject& node)
{
    MStatus           status;
    MFnDependencyNode dep(node, &status);
    if (status.error() || dep.isFromReferencedFile() || dep.isLocked()) {
        return;
    }
    EnsureBoolAttr(
        dep, kAttrUsePhysicalCamera, kAttrUsePhysicalCameraShort, kUiUsePhysicalCamera, false);
    EnsureEnumAttr(
        dep, kAttrFStopPreset, kAttrFStopPresetShort, kUiFStopPreset, kFStopLabels, kFStopCount,
        kFStopDefault);
    EnsureFloatAttr(dep, kAttrFStopCustom, kAttrFStopCustomShort, kUiFStopCustom, 4.0f);
    EnsureEnumAttr(
        dep, kAttrShutterPreset, kAttrShutterPresetShort, kUiShutterPreset, kShutterLabels,
        kShutterCount, kShutterDefault);
    EnsureFloatAttr(
        dep, kAttrShutterCustom, kAttrShutterCustomShort, kUiShutterCustom, 1.0f / 125.0f);
    EnsureEnumAttr(
        dep, kAttrISOPreset, kAttrISOPresetShort, kUiISOPreset, kISOLabels, kISOCount, kISODefault);
    EnsureFloatAttr(dep, kAttrISOCustom, kAttrISOCustomShort, kUiISOCustom, 400.0f);
    EnsureFloatAttr(dep, kAttrEVComp, kAttrEVCompShort, kUiEVComp, 0.0f);
}

} // namespace MayaHydraCameraExposure

#endif // MAYAHYDRALIB_CAMERA_EXPOSURE_H
