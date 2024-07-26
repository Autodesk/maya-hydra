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

/* This class is an example on how to add Hydra primitives into a Hydra viewport. We add a grid of cubes meshes as primitives.
    We are using a HdRetainedSceneIndex as it contains helper functions to add/remove/dirty prims.
    We could also have done a subclass of HdRetainedSceneIndex as well.
*/

//Local headers
#include "fvpDataProducerSceneIndexExample.h"

#ifdef CODE_COVERAGE_WORKAROUND
#include <flowViewport/fvpUtils.h>
#endif

//USD headers
#include <pxr/base/gf/range3d.h>
#include <pxr/base/gf/bbox3d.h>
#include <pxr/base/vt/array.h>

//Hydra headers
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/instanceSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/extentSchema.h>

//TBB headers to use multithreading
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/blocked_range3d.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace PrototypeInstancing
{
    //Global functions to deal with geometry prototype instancing

    // Returns a typed sampled data source for a small number of VtArray types.
    HdSampledDataSourceHandle getRetainedDataSource(VtValue const &val)
    {
        if (val.IsHolding<int>()) {
            return HdRetainedTypedSampledDataSource<int>::New(
                val.UncheckedGet<int>());
        }
        if (val.IsHolding<VtIntArray>()) {
            return HdRetainedTypedSampledDataSource<VtIntArray>::New(
                val.UncheckedGet<VtIntArray>());
        }
        if (val.IsHolding<VtMatrix4dArray>()) {
            return HdRetainedTypedSampledDataSource<VtMatrix4dArray>::New(
                val.UncheckedGet<VtMatrix4dArray>());
        }
        if (val.IsHolding<VtFloatArray>()) {
            return HdRetainedTypedSampledDataSource<VtFloatArray>::New(
                val.UncheckedGet<VtFloatArray>());
        }
        if (val.IsHolding<VtVec3fArray>()) {
            return HdRetainedTypedSampledDataSource<VtVec3fArray>::New(
                val.UncheckedGet<VtVec3fArray>());
        }

        TF_WARN("Unsupported primvar type %s",
                val.GetTypeName().c_str());
        return HdRetainedTypedSampledDataSource<VtValue>::New(val);
    }

    HdContainerDataSourceHandle constructPrimvarDataSource(const VtValue& value, const TfToken& interpolation, const TfToken& role)
    {
        static auto emptyArray = HdRetainedTypedSampledDataSource<VtIntArray>::New(VtIntArray());

        return HdPrimvarSchema::BuildRetained(getRetainedDataSource(value),
            HdSampledDataSourceHandle(), emptyArray, //is an indexer on the primVars which we don't use, primVars are not indexed in our case.
            HdPrimvarSchema::BuildInterpolationDataSource(interpolation),
            HdPrimvarSchema::BuildInterpolationDataSource(role));
    }

    //Create an instancer topology data source for the instancer, and supply the matrices as a by-instance varying primvar.
    void createInstancer(const SdfPath& id,
        const SdfPath& prototypeId, const VtIntArray& prototypeIndices,
        const VtMatrix4dArray& matrices, HdRetainedSceneIndexRefPtr& retainedScene)
    {
        auto instanceIndices = Fvp::DataProducerSceneIndexExample::_InstanceIndicesDataSource::New(std::move(prototypeIndices));

        HdDataSourceBaseHandle instancerTopologyData =
            HdInstancerTopologySchema::Builder()
                .SetPrototypes(HdRetainedTypedSampledDataSource<VtArray<SdfPath>>::New({ prototypeId }))
                .SetInstanceIndices(instanceIndices)
                .Build();

        //The matrices are varying per instance
        HdDataSourceBaseHandle primvarData = constructPrimvarDataSource(VtValue(matrices), HdPrimvarSchemaTokens->instance, HdInstancerTokens->instanceTransforms);

        HdDataSourceBaseHandle primvarsDs = HdRetainedContainerDataSource::New(
            HdInstancerTokens->instanceTransforms, primvarData);

        HdRetainedContainerDataSourceHandle instancerData =
            HdRetainedContainerDataSource::New(
            HdInstancerTopologySchema::GetSchemaToken(),    instancerTopologyData, 
            HdPrimvarsSchema::GetSchemaToken(),             primvarsDs);
    
        // Add the primitives to the scene index
        retainedScene->AddPrims({ { id, HdInstancerTokens->instancer, instancerData } });
    }
}

namespace FVP_NS_DEF {

DataProducerSceneIndexExample::DataProducerSceneIndexExample() :
    _cubeRootPath(SdfPath("/cube_")),//Is the root path for the cubes
    _instancerPath(SdfPath("/instancer_"))//Is the instancer path when using instancing
{
    //Create the HdRetainedSceneIndex to be able to easily add primitives
    _retainedSceneIndex = HdRetainedSceneIndex::New();

    _retainedSceneIndex->SetDisplayName("Flow Viewport Data Producer Example Scene Index");
    
    //Add all primitives
    _AddAllPrims();
}

DataProducerSceneIndexExample::~DataProducerSceneIndexExample() 
{ 
#ifdef CODE_COVERAGE_WORKAROUND
    Fvp::leakSceneIndex(_retainedSceneIndex);
#endif

    removeDataProducerSceneIndex();
    _hydraInterface = nullptr;
}

void DataProducerSceneIndexExample::setCubeGridParams(const CubeGridCreationParams& params) 
{
    if (params == _currentCubeGridParams){
        return;
    }

    _RemoveAllPrims();//Use old params from _currentCubeGridParams
    _currentCubeGridParams = params;//Update _currentCubeGridParams 
    _AddAllPrims();//Use new params from updated _currentCubeGridParams 
}

// Compute the resulting axis aligned bounding box (AABB) of the 3D grid of cube primitives, is used by the DCC node to give its bounding box
void DataProducerSceneIndexExample::getPrimsBoundingBox(float& corner1X, float& corner1Y, float& corner1Z, 
                                                     float& corner2X, float& corner2Y, float& corner2Z)const
{
    //Compute the initial cube prim AABB
    const GfRange3d cubeRange    (
                                    {-_currentCubeGridParams._halfSize, -_currentCubeGridParams._halfSize, -_currentCubeGridParams._halfSize},     
                                    { _currentCubeGridParams._halfSize,  _currentCubeGridParams._halfSize,  _currentCubeGridParams._halfSize}
                                 );
    const GfBBox3d  cubeInitialBBox (cubeRange, _currentCubeGridParams._initialTransform);
    
    //Init some variables before looping
    const GfVec3d initTrans   = _currentCubeGridParams._initialTransform.ExtractTranslation();
    const int numLevelsX      = _currentCubeGridParams._numLevelsX;
    const int numLevelsY      = _currentCubeGridParams._numLevelsY;
    const int numLevelsZ      = _currentCubeGridParams._numLevelsZ;

    //Will hold the combined AABB of all cube prims
    GfBBox3d combinedAABBox(cubeInitialBBox);

    //Combine the AABB of each cube prim of the 3D grid of Hydra cubes primitives
    for (int z = 0; z < numLevelsZ; ++z) {
        for (int y = 0; y < numLevelsY; ++y) {
            for (int x = 0; x < numLevelsX; ++x) {
                //Update translation, by getting the initial transform
                GfMatrix4d currentXForm = _currentCubeGridParams._initialTransform;
                //And updating the translation only in the matrix, _currentCubeGridParams._deltaTrans holds the number of units to separate the cubes in the grid in X, Y, and Z.
                currentXForm.SetTranslateOnly(initTrans + (GfCompMult(_currentCubeGridParams._deltaTrans, GfVec3f(x,y,z))));

                const GfBBox3d currentCubeAABB (cubeRange, currentXForm);
                combinedAABBox = GfBBox3d::Combine(currentCubeAABB, combinedAABBox);
            }
        }
    }

    //Get resulting AABB 
    const GfRange3d resultedAABB    = combinedAABBox.ComputeAlignedRange();
    const GfVec3d& minAABB          = resultedAABB.GetMin();
    const GfVec3d& maxAABB          = resultedAABB.GetMax();

    corner1X                        = minAABB.data()[0];
    corner1Y                        = minAABB.data()[1];
    corner1Z                        = minAABB.data()[2];

    corner2X                        = maxAABB.data()[0];
    corner2Y                        = maxAABB.data()[1];
    corner2Z                        = maxAABB.data()[2];
}

void DataProducerSceneIndexExample::_AddAllPrims()
{
    if (_isEnabled || ! _retainedSceneIndex){
        return;
    }

    if (_currentCubeGridParams._useInstancing) { 
        _AddAllPrimsWithInstancing();
    } else{
        _AddAllPrimsNoInstancing();
    }

    _isEnabled = true;
}

void DataProducerSceneIndexExample::_AddAllPrimsWithInstancing()
{
    static const bool instancing = true;

    //Copy the main cube primitive in the array, we will update only the SdfPath and the transform, all others attributes are identical
    HdRetainedSceneIndex::AddedPrimEntry cubePrimEntry = _CreateCubePrim(_cubeRootPath, _currentCubeGridParams._halfSize, 
                                                                        _currentCubeGridParams._color, _currentCubeGridParams._opacity, 
                                                                        _currentCubeGridParams._initialTransform, instancing);

    //Add the cube to the retained scene index
    _retainedSceneIndex->AddPrims({cubePrimEntry});

    const size_t totalSize = _currentCubeGridParams._numLevelsX * _currentCubeGridParams._numLevelsY * _currentCubeGridParams._numLevelsZ;
    
    const GfVec3d initTrans{0,0,0};//             = _currentCubeGridParams._initialTransform.ExtractTranslation();
    const int numLevelsX                = _currentCubeGridParams._numLevelsX;
    const int numLevelsY                = _currentCubeGridParams._numLevelsY;
    const int numLevelsZ                = _currentCubeGridParams._numLevelsZ;

    VtIntArray      prototypeIndices(totalSize, 0);//Resize and set the value of all elements to 0 which is our prototype index since we only have one prototype
    GfMatrix4d identity;
    identity = identity.SetIdentity();
    VtMatrix4dArray matrices(totalSize, identity);//Resize and set the initial value
        
    //Create matrices array
    tbb::parallel_for(tbb::blocked_range3d<int, int, int>(0, numLevelsZ, 0, numLevelsY, 0, numLevelsX),
        [&](const tbb::blocked_range3d<int, int, int>& r)
        {
            for (int z = r.pages().begin(), z_end = r.pages().end(); z < z_end; ++z) {
                for (int y = r.rows().begin(), y_end = r.rows().end(); y < y_end; ++y) {
                    for (int x = r.cols().begin(), x_end = r.cols().end(); x < x_end; ++x) {

                        const size_t index = x + (numLevelsX * y) + (numLevelsX * numLevelsY * z);
                        
                        //Update prototype index
                        prototypeIndices[index] = index;

                        //Update translation
                        matrices[index].SetTranslate(initTrans + (GfCompMult(_currentCubeGridParams._deltaTrans, GfVec3f(x,y,z))));
                    }
                }
            }
        }
    );

    //Add new prims to the scene index
    PrototypeInstancing::createInstancer(_instancerPath, _cubeRootPath, prototypeIndices, matrices, _retainedSceneIndex);
}

void DataProducerSceneIndexExample::_AddAllPrimsNoInstancing()
{
    constexpr bool instancing = false;

    //Readability shorthand.
    const auto& cgp = _currentCubeGridParams;

    //Array of added prims.  We want to fill in our addedPrims vector in
    //parallel.  Pre-allocate the addedPrims array for the maximum number of
    //cube primitives in the grid.  Hidden cubes reduce the size of the array.
    const size_t maxSize = cgp._numLevelsX * cgp._numLevelsY * cgp._numLevelsZ;
    HdRetainedSceneIndex::AddedPrimEntries addedPrims{maxSize};
    std::atomic<int> nbEntries{0};

    //Init some variables before looping
    const std::string cubeRootString    = _cubeRootPath.GetName();
    const GfVec3d initTrans             = cgp._initialTransform.ExtractTranslation();
    const int numLevelsX                = cgp._numLevelsX;
    const int numLevelsY                = cgp._numLevelsY;
    const int numLevelsZ                = cgp._numLevelsZ;

    //Create the 3D grid of cubes primitives in Hydra
    tbb::parallel_for(tbb::blocked_range3d<int, int, int>(0, numLevelsZ, 0, numLevelsY, 0, numLevelsX),
        [&](const tbb::blocked_range3d<int, int, int>& r)
        {
            for (int z = r.pages().begin(), z_end = r.pages().end(); z < z_end; ++z) {
                for (int y = r.rows().begin(), y_end = r.rows().end(); y < y_end; ++y) {
                    for (int x = r.cols().begin(), x_end = r.cols().end(); x < x_end; ++x) {

                        //Compute the prim path so that all cube prim path are unique
                        std::string cubePathStr(
                            cubeRootString + std::to_string(x) +
                            "_" + std::to_string(y) + "_" + std::to_string(z));

                        //If cube is hidden, skip to the next one.
                        if (cgp._hidden.count(cubePathStr) > 0) {
                            continue;
                        }

                        int ndx = nbEntries++;
                        HdRetainedSceneIndex::AddedPrimEntry& cubePrimEntry =
                            addedPrims[ndx];
                        // Prims added to retained scene index must have
                        // absolute path, otherwise infinite recursion.
                        auto cubePath = SdfPath::AbsoluteRootPath().AppendChild(TfToken(cubePathStr));
                        cubePrimEntry = _CreateCubePrim(
                            cubePath, cgp._halfSize, cgp._color,
                            cgp._opacity, cgp._initialTransform, instancing);

                        //Update translation, by getting the initial transform
                        GfMatrix4d currentXForm = cgp._initialTransform;

                        //Is the cube in set of transformed cubes?  Currently
                        //only supporting translation, so add the translation
                        //to the transform if appropriate.
                        auto cubeTrans = initTrans;
                        auto found = cgp._transformed.find(cubePathStr);
                        if (found != cgp._transformed.end()) {
                            cubeTrans += found->second;
                        }

                        //And updating the translation only in the matrix, _currentCubeGridParams._deltaTrans holds the number of units to separate the cubes in the grid in X, Y, and Z.
                        currentXForm.SetTranslateOnly(cubeTrans + (GfCompMult(cgp._deltaTrans, GfVec3f(x,y,z))));

                        //Update the matrix in the data source for this cube prim
                        cubePrimEntry.dataSource                             = HdContainerDataSourceEditor(cubePrimEntry.dataSource)
                                                                                        .Set(HdXformSchema::GetDefaultLocator(), 
                                                                                        HdXformSchema::Builder().SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(currentXForm))
                                                                                        .Build())
                                                                                        .Finish();
                    }
                }
            }
        }
    );

    //All done, bring the added entries vector down to size.
    addedPrims.resize(nbEntries);
    
    //Add all the cube prims to the retained scene index
    _retainedSceneIndex->AddPrims(addedPrims);
}

//Removes the cube prims from the scene index
void DataProducerSceneIndexExample::_RemoveAllPrims()
{
    if(! _retainedSceneIndex || ! _isEnabled)return;

    if (_currentCubeGridParams._useInstancing) { 
        _RemoveAllPrimsWithInstancing();
    }
    else{
        _RemoveAllPrimsNoInstancing();
    }
}

void DataProducerSceneIndexExample::_RemoveAllPrimsNoInstancing()
{
    //We delete a 3D grid of cubes
    const size_t totalSize = _currentCubeGridParams._numLevelsX * _currentCubeGridParams._numLevelsY * _currentCubeGridParams._numLevelsZ;
    
    HdSceneIndexObserver::RemovedPrimEntries removedEntries;
    HdSceneIndexObserver::RemovedPrimEntry removedEntry(_cubeRootPath);
   
    //Resize the removedEntries array to the exact size of the number of cube primitives we want to remove from the 3D grid.
    removedEntries.resize(totalSize, removedEntry);
    
    //Init some variables before looping
    const std::string cubeRootString = _cubeRootPath.GetString();
    const int numLevelsX             = _currentCubeGridParams._numLevelsX;
    const int numLevelsY             = _currentCubeGridParams._numLevelsY;
    const int numLevelsZ             = _currentCubeGridParams._numLevelsZ;

    //Remove cube primitives in Hydra
    tbb::parallel_for(tbb::blocked_range3d<int, int, int>(0, numLevelsZ, 0, numLevelsY, 0, numLevelsX),
        [&](const tbb::blocked_range3d<int, int, int>& r)
        {
            for (int z = r.pages().begin(), z_end = r.pages().end(); z < z_end; ++z) {
                for (int y = r.rows().begin(), y_end = r.rows().end(); y < y_end; ++y) {
                    for (int x = r.cols().begin(), x_end = r.cols().end(); x < x_end; ++x) {
                        
                        //Get the SdfPath from the cube prim at that place in the 3D grid. It's the same way as when we create the cube prim, see DataProducerSceneIndexExample::AddAllPrimsNoInstancing()
                        const SdfPath currentCubePath(cubeRootString + std::to_string(x) + std::string("_") + std::to_string(y)+ std::string("_") + std::to_string(z));
                        
                        //Store the SdfPath of the cube prim to remove
                        removedEntries[x + (numLevelsX * y) + (numLevelsX * numLevelsY * z)].primPath = currentCubePath;
                    }
                }
            }
        }
    );

    //Remove all the cube prims from the retained scene index
    _retainedSceneIndex->RemovePrims(removedEntries);

    _isEnabled = false;
}

void DataProducerSceneIndexExample::_RemoveAllPrimsWithInstancing()
{
    //In the case of instancing, there were only 2 things added, the cube and the instancer
    _retainedSceneIndex->RemovePrims({{_cubeRootPath}, {_instancerPath}});

    _isEnabled = false;
}

//Create a Hydra cube primitive from these parameters
HdRetainedSceneIndex::AddedPrimEntry DataProducerSceneIndexExample::_CreateCubePrim(const SdfPath& cubePath, float halfSize, const GfVec3f& displayColor, 
                                                                                float opacity, const GfMatrix4d& transform, bool instanced)const
{
    using _PointArrayDs = HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>;
    using _IntArrayDs   = HdRetainedTypedSampledDataSource<VtIntArray>;

    //Cube hardcoded information
    static const VtIntArray faceVertexCounts    = {4, 4, 4, 4, 4, 4}; //Using quads
    static const VtIntArray faceVertexIndices   = {0, 1, 3, 2, 2, 3, 5, 4, 4, 5, 7, 6, 6, 7, 1, 0, 1, 7, 5, 3, 6, 0, 2, 4};

    const _IntArrayDs::Handle fvcDs             = _IntArrayDs::New(faceVertexCounts);
    const _IntArrayDs::Handle fviDs             = _IntArrayDs::New(faceVertexIndices);

    //Vertices of the cube
    const VtArray<GfVec3f> points = {
                                        {-halfSize, -halfSize,  halfSize},
                                        { halfSize, -halfSize,  halfSize},
                                        {-halfSize,  halfSize,  halfSize},
                                        { halfSize,  halfSize,  halfSize},
                                        {-halfSize,  halfSize, -halfSize},
                                        { halfSize,  halfSize, -halfSize},
                                        {-halfSize, -halfSize, -halfSize},
                                        { halfSize, -halfSize, -halfSize}
                                    };

    const HdContainerDataSourceHandle meshDs =
            HdMeshSchema::Builder()
                .SetTopology(HdMeshTopologySchema::Builder()
                    .SetFaceVertexCounts(fvcDs)
                    .SetFaceVertexIndices(fviDs)
                    .Build())
                .Build();

    const HdContainerDataSourceHandle primvarsDs =
        HdRetainedContainerDataSource::New(
            
            //Create the vertices positions
            HdPrimvarsSchemaTokens->points,
            HdPrimvarSchema::Builder()
                .SetPrimvarValue(_PointArrayDs::New(points))
                .SetInterpolation(HdPrimvarSchema::
                    BuildInterpolationDataSource(
                        HdPrimvarSchemaTokens->vertex))
                .SetRole(HdPrimvarSchema::
                    BuildRoleDataSource(
                        HdPrimvarSchemaTokens->point))
                .Build(),

            //Create the vertex colors
            HdTokens->displayColor,
            HdPrimvarSchema::Builder()
                .SetIndexedPrimvarValue(
                    HdRetainedTypedSampledDataSource<VtVec3fArray>::New(
                        VtVec3fArray{
                            displayColor, //Feel free to add more colors if needed, we only have one in this example.
                        }))
                .SetIndices(
                    HdRetainedTypedSampledDataSource<VtIntArray>::New(
                        VtIntArray{
                            0, 0, 0, 0, 0, 0, 0, 0, //Is an index in the per vertex color array above
                        }
                    )
                )
                .SetInterpolation(
                    HdPrimvarSchema::BuildInterpolationDataSource(
                        HdPrimvarSchemaTokens->varying))
                .SetRole(
                    HdPrimvarSchema::BuildRoleDataSource(
                        HdPrimvarSchemaTokens->color))//vertex color
                .Build(),

            //Create a face vertex opacity
            HdTokens->displayOpacity,
            HdPrimvarSchema::Builder()
                .SetPrimvarValue(
                    HdRetainedTypedSampledDataSource<VtFloatArray>::New(
                        VtFloatArray(24, opacity))) //Is a value per face vertex (quads)
                .SetInterpolation(
                    HdPrimvarSchema::BuildInterpolationDataSource(
                        HdPrimvarSchemaTokens->faceVarying))
                .Build()
        );

    const GfRange3d cubeRange ({-halfSize, -halfSize, -halfSize},     
                               { halfSize,  halfSize,  halfSize});

    //Add the cube primitive
    HdRetainedSceneIndex::AddedPrimEntry addedPrim;
    addedPrim.primPath      = cubePath;
    addedPrim.primType      = HdPrimTypeTokens->mesh;
    if (instanced){

        const HdDataSourceBaseHandle instancedByData =
            HdInstancedBySchema::Builder()
            .SetPaths(HdRetainedTypedSampledDataSource<VtArray<SdfPath>>::New(VtArray<SdfPath>({ _instancerPath })))
            .Build();

        addedPrim.dataSource = HdRetainedContainerDataSource::New(
        
                        //Create a matrix
                        HdXformSchemaTokens->xform,
                        HdXformSchema::Builder()
                                .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(
                                transform))
                                .SetResetXformStack(HdRetainedTypedSampledDataSource<bool>::New(true)) //Mark the transform of prototype not inherit from parent
                                .Build(),

                        //Create an extent attribute to support the viewport bounding box display style, 
                        //if no extent attribute is added, it will not be displayed at all in bounding box display style
                        HdExtentSchemaTokens->extent,
                        HdExtentSchema::Builder()
                            .SetMin(HdRetainedTypedSampledDataSource<GfVec3d>::New(cubeRange.GetMin()))
                            .SetMax(HdRetainedTypedSampledDataSource<GfVec3d>::New(cubeRange.GetMax()))
                            .Build(),

                        //create a mesh
                        HdMeshSchemaTokens->mesh,
                        meshDs,
                        HdPrimvarsSchemaTokens->primvars,
                        primvarsDs,

                        HdInstancedBySchema::GetSchemaToken(), //Add the instancer path in the HdInstancedBySchema
                        instancedByData
                    );
    }else{

        addedPrim.dataSource = HdRetainedContainerDataSource::New(
        
                        //Create a matrix
                        HdXformSchemaTokens->xform,
                        HdXformSchema::Builder()
                                .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(
                                transform)).Build(),

                        //create an extent attribute to easily compute the bounding box from it
                        HdExtentSchemaTokens->extent,
                        HdExtentSchema::Builder()
                            .SetMin(HdRetainedTypedSampledDataSource<GfVec3d>::New(cubeRange.GetMin()))
                            .SetMax(HdRetainedTypedSampledDataSource<GfVec3d>::New(cubeRange.GetMax()))
                            .Build(),

                        //create a mesh
                        HdMeshSchemaTokens->mesh,
                        meshDs,
                        HdPrimvarsSchemaTokens->primvars,
                        primvarsDs
                    );
    }

    return addedPrim;
}

void DataProducerSceneIndexExample::addDataProducerSceneIndex(const PXR_NS::SdfPath& prefix)
{
    if (!_dataProducerSceneIndexAdded && _hydraInterface){
        const bool res = _hydraInterface->addDataProducerSceneIndex(_retainedSceneIndex, prefix, _containerNode, PXR_NS::FvpViewportAPITokens->allViewports, PXR_NS::FvpViewportAPITokens->allRenderers);
        if (false == res){
            TF_CODING_ERROR("_hydraInterface->addDataProducerSceneIndex returned false !");
        }
        _dataProducerSceneIndexAdded = true;
    }
}

void DataProducerSceneIndexExample::removeDataProducerSceneIndex() 
{ 
    if (_dataProducerSceneIndexAdded && _hydraInterface){
        _hydraInterface->removeViewportDataProducerSceneIndex(_retainedSceneIndex, PXR_NS::FvpViewportAPITokens->allViewports);
        _dataProducerSceneIndexAdded = false;
    }
}

} //End of namespace FVP_NS_DEF
