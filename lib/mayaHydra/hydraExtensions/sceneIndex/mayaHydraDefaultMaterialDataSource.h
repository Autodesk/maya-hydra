//
// Copyright 2024 Autodesk, Inc. All rights reserved.
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

#ifndef MAYAHYDRADEFAULTMATERIALDATASOURCE_H
#define MAYAHYDRADEFAULTMATERIALDATASOURCE_H

#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/enums.h>

PXR_NAMESPACE_OPEN_SCOPE

class MayaHydraSceneIndex;

/**
 * \brief A container data source representing a default material with USDPreviewSurface
 */
 class MayaHydraDefaultMaterialDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(MayaHydraDefaultMaterialDataSource);

    // ------------------------------------------------------------------------
    // HdContainerDataSource implementations
    TfTokenVector GetNames() override;
    HdDataSourceBaseHandle Get(const TfToken& name) override;

private:
    MayaHydraDefaultMaterialDataSource(
        const SdfPath& id,
        TfToken type,
        MayaHydraSceneIndex* sceneIndex);

    HdDataSourceBaseHandle _GetMaterialDataSource();    
    HdDataSourceBaseHandle _GetMaterialBindingDataSource();

    SdfPath _id;
    TfToken _type;
    MayaHydraSceneIndex* _sceneIndex = nullptr;
};

HD_DECLARE_DATASOURCE_HANDLES(MayaHydraDefaultMaterialDataSource);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRADEFAULTMATERIALDATASOURCE_H
