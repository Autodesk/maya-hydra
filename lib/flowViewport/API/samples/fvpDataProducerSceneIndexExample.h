//
// Copyright 2023 Autodesk
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
#ifndef FLOW_VIEWPORT_EXAMPLES_DATA_PRODUCER_SCENE_INDEX_EXAMPLE_H
#define FLOW_VIEWPORT_EXAMPLES_DATA_PRODUCER_SCENE_INDEX_EXAMPLE_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/API/fvpDataProducerSceneIndexInterface.h"

//USD/Hydra headers
#include <pxr/base/tf/refPtr.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/retainedDataSource.h>

namespace FVP_NS_DEF {

/** This class is an example on how to inject Hydra prims into a Hydra viewport. 
*   An data producer scene index is a scene index that adds primitives to the current rendering.
*   We are holding a HdRetainedSceneIndex in this class, as it contains helper functions to add/remove/dirty prims.
*   Another possibility is that we could also have subclassed HdRetainedSceneIndex.
*   We are creating a 3D grid of cubes using Hydra mesh primitives.
*/
class FVP_API DataProducerSceneIndexExample
{
public:
	DataProducerSceneIndexExample();
	~DataProducerSceneIndexExample();

    ///Set the hydra interface
    void setHydraInterface(DataProducerSceneIndexInterface* hydraInterface) {_hydraInterface = hydraInterface;}

    /**Is called by the DCC node to set its node pointer.
    * Is also our triggering function to add the data producer scene index as we want to wait to have the DCC node pointer value initialized before dataProducer the scene index.
    */
    void setContainerNode(void* node){_containerNode = node;}
    void setContainerNodeInverseTransform(const PXR_NS::GfMatrix4d& InvTransform);

    /// Compute the resulting axis aligned bounding box of the 3D grid of cube primitives, is used by the DCC node to give its bounding box
    void getPrimsBoundingBox(float& corner1X, float& corner1Y, float& corner1Z, float& corner2X, float& corner2Y, float& corner2Z)const;

    /// Are the creation parameters for a 3D Grid of Hydra cube primitives
    struct CubeGridCreationParams
    {
        /// Constructor
        CubeGridCreationParams(){
            _numLevelsX = 10;//Default values
            _numLevelsY = 10;
            _numLevelsZ = 1;
            _halfSize   = 2.0;
            _color      = {0.f, 1.0f, 0.0f};
            _opacity    = 0.8;
            _deltaTrans = {5.0f, 5.0f, 5.0f};
            _initalTransform.SetIdentity();
            _useInstancing = false;
        }

        //Comparison operator
        bool operator == (const CubeGridCreationParams& other)const{
            return  (_numLevelsX        == other._numLevelsX) && 
                    (_numLevelsY        == other._numLevelsY) && 
                    (_numLevelsZ        == other._numLevelsZ) && 
                    (_halfSize          == other._halfSize) && 
                    (_color             == other._color) &&
                    (_opacity           == other._opacity) &&
                    (_initalTransform   == other._initalTransform)&&
                    (_deltaTrans        == other._deltaTrans) &&
                    (_useInstancing     == other._useInstancing);
        }

        /// Number of X levels for the 3D grid of cube prims
        int                 _numLevelsX{10};
        /// Number of Y levels for the 3D grid of cube prims
        int                 _numLevelsY{10};
        /// Number of Z levels for the 3D grid of cube prims
        int                 _numLevelsZ{1};
        /// Half size of each cube in the 3D grid.
        double              _halfSize{2.0};
        /// Color of each cube in the 3D grid.
        PXR_NS::GfVec3f     _color{0.f, 1.0f, 0.0f};
        /// Opacity of each cube in the 3D grid.
        double              _opacity{0.8};
        /// Initial transform of each cube in the 3D grid.
        PXR_NS::GfMatrix4d  _initalTransform;
        /** _deltaTrans.x is the space between 2 cubes on the X axis of the 3D grid.
        *   _deltaTrans.y is the space between 2 cubes on the Y axis of the 3D grid.
        *   _deltaTrans.z is the space between 2 cubes on the Z axis of the 3D grid.
        * */
        PXR_NS::GfVec3f     _deltaTrans{5.0f, 5.0f, 5.0f};
        /// if _useInstancing is true, then we are using Hydra instancing to create the cube prims, if it is false then we are not using Hydra instancing.
        bool                _useInstancing{false};
    };

    ///Set the CubeGridCreationParams
    void setCubeGridParams(const CubeGridCreationParams& params);

    ///Call FlowViewport::DataProducerSceneIndexInterface::_AddDataProducerSceneIndex to add our data producer scene index to create the 3D grid of cubes
    void addDataProducerSceneIndex();
    
    ///Call the FlowViewport::DataProducerSceneIndexInterface::RemoveViewportDataProducerSceneIndex to remove our data producer scene index from the Hydra viewport
    void removeDataProducerSceneIndex();

    /// This class is about geometry (prototype) instancing and holds the instance indices
    class _InstanceIndicesDataSource : public PXR_NS::HdVectorDataSource
    {
    public:
        HD_DECLARE_DATASOURCE(_InstanceIndicesDataSource);
        size_t GetNumElements() override { return 1; }
        PXR_NS::HdDataSourceBaseHandle GetElement(size_t) override
        {
            return PXR_NS::HdRetainedTypedSampledDataSource<PXR_NS::VtIntArray>::New(_indices);
        }
    private:
        _InstanceIndicesDataSource(const PXR_NS::VtIntArray& indices) : _indices(indices) {}
        PXR_NS::VtIntArray const _indices;
    };

protected:

    ///Store the Hydra interface
    DataProducerSceneIndexInterface* _hydraInterface {nullptr};
    
    /// Enabled state of this client to enable/disable the scene indices
    bool    _isEnabled {false};

    /// Container node from a DCC (maya or 3dsmax for example)
    void*  _containerNode {nullptr};//Is a MObject* for Maya or an INode* for 3ds max

    ///Is the container node inverse transform matrix to remove transform matrix being applied twice for instances
    PXR_NS::GfMatrix4d  _containerNodeInvTransform;

    /// Did we already add this data producer scene index to some render index ?
    bool   _dataProducerSceneIndexAdded {false};

    ///Create a Hydra cube primitive
    PXR_NS::HdRetainedSceneIndex::AddedPrimEntry _CreateCubePrim(const PXR_NS::SdfPath& cubePath, float halfSize, 
                                                const PXR_NS::GfVec3f& displayColor, float opacity, const PXR_NS::GfMatrix4d& transform, bool instanced)const;

    ///Create the 3D grid of Hydra cube primitives and add them to the retained scene index
    void _AddAllPrims();

    ///Remove the 3D grid of Hydra cube primitives from the retained scene index
    void _RemoveAllPrims();

    ///Create the 3D grid of Hydra cube primitives and add them to the retained scene index. We are not using Hydra instancing in this function.
    void _AddAllPrimsNoInstancing();
    
    ///Create the 3D grid of Hydra cube primitives and add them to the retained scene index. We are using Hydra instancing in this function.
    void _AddAllPrimsWithInstancing();

    ///Remove the 3D grid of Hydra cube primitives from the retained scene index. We are not using Hydra instancing in this function.
    void _RemoveAllPrimsNoInstancing();
    
    ///Remove the 3D grid of Hydra cube primitives from the retained scene index. We are using Hydra instancing in this function.
    void _RemoveAllPrimsWithInstancing();
    
    ///Aggregation of the retained scene index data producer prims into Hydra
    PXR_NS::HdRetainedSceneIndexRefPtr  _retainedSceneIndex  {nullptr};

    /// 3D grid of cubes primitives parameters
    CubeGridCreationParams              _currentCubeGridParams;

    /// Is the root path to create unique sdfPath for each prim of the 3D grid of cube prims
    PXR_NS::SdfPath                     _cubeRootPath;

    /// Is the instancer path when using instancing
    PXR_NS::SdfPath                     _instancerPath;
};

} //end of namespace FVP_NS_DEF {

#endif //FLOW_VIEWPORT_EXAMPLES_DATA_PRODUCER_SCENE_INDEX_EXAMPLE_H