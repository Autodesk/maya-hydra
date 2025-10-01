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

#include "customPrimvarSceneIndex.h"

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/dataSourceLocator.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>


PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

TF_DEFINE_PRIVATE_TOKENS(
    _tokens, 
    ((aiSubdivUvSmoothing, "aiSubdivUvSmoothing"))
    ((arnoldSmoothing, "arnold::smoothing"))
    );

/// A data source with custom primvars
class _CustomDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_CustomDataSource);

    TfTokenVector GetNames() override
    {
        TfTokenVector result;
        if (_inputPrimDs) {
            result = _inputPrimDs->GetNames();
        }
        return result;
    }

    HdDataSourceBaseHandle Get(const TfToken& name) override
    {
        HdDataSourceBaseHandle result;
        if (_inputPrimDs) {
            result = _inputPrimDs->Get(name);
        }
        if (name == HdPrimvarsSchemaTokens->primvars) {
            if (HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(_inputPrimDs)) {
                result = _MapPrimvars(primvars);
            }
        }
        return result;
    }


    HdDataSourceBaseHandle _MapPrimvars(HdPrimvarsSchema primvars)
    {
        HdContainerDataSourceEditor primvarsEditor(primvars.GetContainer());

        // Map aiSubdivUvSmoothing to arnold:smoothing
        if (HdPrimvarSchema smoothingPrimvar = primvars.GetPrimvar(_tokens->aiSubdivUvSmoothing)) {
            if (!primvars.GetPrimvar(_tokens->arnoldSmoothing)) {
                // Create a primvar with new name but still original value/interpolation/role
                primvarsEditor.Overlay(
                    HdDataSourceLocator(_tokens->arnoldSmoothing),
                    HdPrimvarSchema::Builder()
                    .SetPrimvarValue(smoothingPrimvar.GetPrimvarValue())
                    .SetInterpolation(smoothingPrimvar.GetInterpolation())
                    .SetRole(smoothingPrimvar.GetRole())
                    .Build());
                // Block the original primvar
                primvarsEditor.Set(
                    HdDataSourceLocator(_tokens->aiSubdivUvSmoothing),
                    HdBlockDataSource::New());
            }
        }

        return primvarsEditor.Finish();
    }

protected:
    _CustomDataSource(HdContainerDataSourceHandle const& inputDs)
        : _inputPrimDs(inputDs)
    {
    }

private:
    HdContainerDataSourceHandle _inputPrimDs;
};


HdCustomPrimvarSceneIndex::HdCustomPrimvarSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
: HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
{}

HdCustomPrimvarSceneIndex::~HdCustomPrimvarSceneIndex()
    = default;

HdCustomPrimvarSceneIndexRefPtr
HdCustomPrimvarSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
    return TfCreateRefPtr(new HdCustomPrimvarSceneIndex(
        inputSceneIndex));
}

HdSceneIndexPrim HdCustomPrimvarSceneIndex::GetPrim(const SdfPath& primPath) const
{
    if (HdSceneIndexBaseRefPtr input = _GetInputSceneIndex()) {
        HdSceneIndexPrim prim = input->GetPrim(primPath);

        return { prim.primType, _CustomDataSource::New(prim.dataSource) };
    }
    return { TfToken(), nullptr };
}

SdfPathVector HdCustomPrimvarSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    if (_GetInputSceneIndex()) {
        return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }

    return {};
}

void HdCustomPrimvarSceneIndex::_PrimsAdded(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    _SendPrimsAdded(entries);
}

void HdCustomPrimvarSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    _SendPrimsRemoved(entries);
}

void HdCustomPrimvarSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    _SendPrimsDirtied(entries);
}

}
