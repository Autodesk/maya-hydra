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
// Hydra container data source for custom Maya plugin nodes. Sits under
// the "mayaNode" token on a mayaCustomDagNode prim and exposes the Maya
// node type name and all non-default attribute values as Hydra data sources.
//

#ifndef MAYAHYDRA_CUSTOM_NODE_DATASOURCE_H
#define MAYAHYDRA_CUSTOM_NODE_DATASOURCE_H

#include <mayaHydraLib/api.h>

#include <pxr/imaging/hd/dataSource.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

class MayaHydraCustomDagAdapter;

/**
 * \brief Container data source exposing a custom Maya plugin node's type name
 *        and all attributes under the "mayaNode" schema token.
 *
 * Children:
 *   - "mayaTypeName" -> HdTypedSampledDataSource<TfToken>
 *   - "mayaAttributes" -> HdTypedSampledDataSource<VtDictionary>
 */
class MayaHydraCustomNodeDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(MayaHydraCustomNodeDataSource);

    /// Returns the child data source names: "mayaTypeName" and "mayaAttributes".
    TfTokenVector GetNames() override;

    /// Returns the data source for the given child name.
    /// "mayaTypeName"   -> HdTypedSampledDataSource<TfToken> with the Maya node type.
    /// "mayaAttributes" -> HdTypedSampledDataSource<VtDictionary> with all non-default attrs.
    HdDataSourceBaseHandle Get(const TfToken& name) override;

private:
    MayaHydraCustomNodeDataSource(MayaHydraCustomDagAdapter* adapter);

    MayaHydraCustomDagAdapter* _adapter;
};

HD_DECLARE_DATASOURCE_HANDLES(MayaHydraCustomNodeDataSource);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRA_CUSTOM_NODE_DATASOURCE_H
