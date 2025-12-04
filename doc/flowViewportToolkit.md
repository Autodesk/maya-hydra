# Flow viewport toolkit

The Flow viewport toolkit is an API for clients to customize the Hydra viewport.

## About the API
-   It is based on  **Hydra 2.0** and leverages the scene index mechanisms, it was not designed to be used with a Hydra 1.0 (scene delegate).
-   It is located in the [Flow viewport toolkit folder](../lib/flowViewport/API).

## What can we do with the API ?

 - Add  Hydra primitives which are neither related to a usd stage nor are DCC native data through what we call a **data producer scene index**.
 - Apply a **filtering scene index** to all primitives. Even if they are from another scene index including DCC native data, USD stages or a custom data producer scene index. 
 - Get information about the Hydra viewports and which render delegate they use
 - Get the version of the API
 
 Each of the domain above is a separate C++ interface with only a few functions in it.

## What is the Flow viewport toolkit merging scene index ?

To be able to do filtering on all data from the DCC native data, USD stages and custom data producer scene indices adding new primitives, we use a merging scene index which gathers all scene indices into a single scene index, please see the following :

![Flow viewport merging scene index schema](./images/fvpMergingSceneIndex.png)

It's not the native Hydra merging scene index which you find in the HdRenderIndex class.
You may also have a look at the implementation of the *[MergingSceneIndex](../lib/flowViewport/sceneIndex/fvpMergingSceneIndex.cpp)* class.

## Filtering Hydra primitives

Hydra has the notion of a filtering scene index. It is a scene index which takes as an input another scene index. The input scene index can be of any type : a retained scene index, a data producer scene index, another filtering scene index etc.  So you create a chain of scene indices.

When the filtering scene index is asked to provide a certain primitive in its *GetPrim* method, it looks into the input scene index for that primitive, it can potentially modify the primitive's attributes or completely replace it by another custom primitive or just return the input scene index's primitive unmodified.
You can have a look at [an example of filtering scene index](../lib/flowViewport/API/samples/fvpFilteringSceneIndexExample.cpp).

The interface to filter primitives is called *[FilteringSceneIndexInterface](../lib/flowViewport/API/fvpFilteringSceneIndexInterface.h)* which is used to register/unregister a *[FilteringSceneIndexClient](../lib/flowViewport/API/fvpFilteringSceneIndexClient.h)*.

To get an instance of the *FilteringSceneIndexInterface* class, please use :

    Fvp::FilteringSceneIndexInterface& filteringSceneIndexInterface = Fvp::FilteringSceneIndexInterface::get();

The filtering scene indices are applied after the Flow viewport toolkit [MergingSceneIndex](../lib/flowViewport/sceneIndex/fvpMergingSceneIndex.cpp), so they apply to all scene indices (USD Stages scene indices, DCC native data scene indices, custom data producer scene indices).

The *[FilteringSceneIndexClient](../lib/flowViewport/API/fvpFilteringSceneIndexClient.h)* class is a functor. It is not owned by the *FilteringSceneIndexInterface* when you register it.

It contains a :
- display name which is a name  associated with your filtering scene index, it is only used as an information and can be anything.
- category which is a container in which you want your filtering scene index (or scene index chain) to go to. Note : the filtering scene indices inside a Category don't have any specific priority when they are called.
-   The renderer names to which your filtering scene index applies. Say "GL, Arnold" for applying this filter to Storm and the Arnold render delegates. Note : using *FvpViewportAPITokens->allRenderers* means applies to all render delegates.
-   A DCC node as a *void** , dccNode is specific to a DCC (like Maya). For Maya it is a *MObject** DAG node. If you provide a non null pointer, we automatically track some events from the DCC node such as node deleted/undeleted or the visibility attribute updates.  When the node is present and visible, we automatically apply the filtering scene index. And when it  is deleted or not visible, we remove the filtering scene index, so no filtering is happening in that case.  It is a convenient way for you to control the filtering through a Maya node. If it is a *nullptr*, we always apply the filtering scene index until you unregister it.

The implementation can be found in the *[FilteringSceneIndexInterfaceImp](../lib/flowViewport/API/interfacesImp/fvpFilteringSceneIndexInterfaceImp.cpp)*  and in the *[FilteringSceneIndicesChainManager](../lib/flowViewport/API/renderViewData/fvpFilteringSceneIndicesChainManager.cpp)* classes.

## Adding Hydra primitives
To add new primitives in a viewport, we created an interface called *[DataProducerSceneIndexInterface](../lib/flowViewport/API/fvpDataProducerSceneIndexInterface.h)*. It is used to manage data producer scene indices in a Hydra viewport. A data producer scene index is a scene index that adds primitives. 
These new primitives are created without the need of a DCC native object or a USD stage. You can create them from scratch.
To get an instance of the DataProducerSceneIndexInterface class, please use :

    Fvp::DataProducerSceneIndexInterface& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
This interface lets you :
-   Add a data producer scene index
-   Remove a data producer scene index previously added through this interface

The data producer scene index added will be merged by our [Flow viewport merging scene index](../lib/flowViewport/sceneIndex/fvpMergingSceneIndex.cpp) so it gets populated with all primitives coming from the DCC native data, USD stages or other custom data producer scene indices.
When adding primitives through *[Fvp::DataProducerSceneIndexInterface::addDataProducerSceneIndex](../lib/flowViewport/API/fvpDataProducerSceneIndexInterface.h#L75)*
You provide :

 - a *HdSceneIndexBaseRefPtr* which is the scene index producing primitives (it could also be the last scene index of a scene index chain, as soon as it creates some primitives)
 - a *SdfPath* which is a prefix you want to add to all your data producer scene index primitives. Note : if you don't want any prefix, pass *SdfPath::AbsoluteRootPath()* to this parameter.
 - a dccNode is a *MObject** from a DAG node for Maya. If you provide a no null pointer, we automatically track some events from attributes such as transform or visibility updated and apply this change to the primitives from the data producer scene index. If the node gets deleted, we remove the scene index primitives from the merging scene index. If this parameter is a *nullptr*, we won't do anything if the node's attributes changes or the node gets deleted. Basically, this is a way for you to set the DCC node as a parent node for all your primitives from the scene index.
 - a viewId which is a Hydra viewport string identifier to which your data producer scene index needs to be associated to. This is a way to add your primitives to only one viewport. Note : set it to *PXR_NS::FvpViewportAPITokens->allRenderViews* to add this data producer scene index to all viewports. To retrieve a specific hydra viewport identifier, please use the *[InformationInterface](../lib/flowViewport/API/fvpInformationInterface.h)* class.
 - a rendererNames which are the Hydra renderer (render delegate)  names to which this scene index should be added. This is only used when viewId above is set to *PXR_NS::FvpViewportAPITokens->allRenderViews*, meaning you want to add this scene index to all viewports that are using these renderers. Note : to apply to multiple renderers, use a separator such as : "GL, Arnold". We are actually looking for the render delegate's name in this string. Set this parameter to *PXR_NS::FvpViewportAPITokens->allRenderers* to add your scene index to all viewports whatever their renderer is.
An example of data producer scene index can be found in [DataProducerSceneIndexExample](../lib/flowViewport/API/samples/fvpDataProducerSceneIndexExample.cpp).

## Get Hydra viewports information
The interface to get Hydra viewports information is called *[InformationInterface](../lib/flowViewport/API/fvpInformationInterface.h)*.
To get an instance of the InformationInterface class, please use :

    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();

It can be used to  :
 - Register /unregister a functor which is the *[Fvp::InformationClient](../lib/flowViewport/API/fvpInformationClient.h)* class to register callbacks when a new Hydra viewport is created/deleted.
 - Get the existing Hydra viewport information via the [Fvp::InformationInterface::GetAllRenderViewDescs](../lib/flowViewport/API/fvpInformationInterface.h#L109) method

The information we provide from a Hydra viewport is in the  *[Fvp::InformationInterface::RenderViewDesc](../lib/flowViewport/API/fvpInformationInterface.h#L43)* struct and contains (at the time of this writing):

 - a viewId which is a Hydra viewport string identifier which is unique for all hydra viewports during a session.
 - a isViewport which indicates if this is an interactive viewport or not (the API is also used by batch rendering, which creates render views that are not viewports).
 - a rendererName which is the Hydra viewport renderer name (example : "GL" for Storm or "Arnold" for the Arnold render delegate)

This struct may be extended in the future to contain more information.

## Get Flow viewport toolkit version
This is to get the version of the  API through the *[Fvp::VersionInterface](../lib/flowViewport/API/fvpVersionInterface.h)* class.
To get an instance of the *VersionInterface* class, please use :

    Fvp::VersionInterface& versionInterface = Fvp::VersionInterface::Get();

You get the version as semantic versioning  : majorVersion, minorVersion and patchLevel.

## Samples
The API contains examples which are Maya projects on how to filter and add primitives and get viewport information, please see [Flow viewport API examples](../lib/mayaHydra/flowViewportAPIExamples) :

| Location      | Description                                                                                   |
|-------------  |---------------------------------------------------------------------------------------------  |
| [lib/flowViewport/usdPlugins/shadersDiscoveryPlugin](https://github.com/Autodesk/maya-hydra/tree/dev/lib/flowViewport/usdPlugins/shadersDiscoveryPlugin) | Is an example on how to develop a custom GLSL shader for Hydra Storm which you can apply later on USD / Hydra primitives |
| [lib/mayaHydra/flowViewportAPIExamples/customShadersNode](https://github.com/Autodesk/maya-hydra/tree/dev/lib/mayaHydra/flowViewportAPIExamples/customShadersNode) | Is a Maya node which creates an Hydra primitive and applies the custom GLSL shader for Hydra Storm |
| [lib/mayaHydra/flowViewportAPIExamples/flowViewportAPILocator](https://github.com/Autodesk/maya-hydra/tree/dev/lib/mayaHydra/flowViewportAPIExamples/flowViewportAPILocator) | Is a Maya node which creates Hydra primitives to display a grid of cubes and applies a filtering scene index to remove primitives with more than a certain number of vertices.<BR>This example shows how to create a Hydra mesh primitive, how to use Hydra instancing and deal with selection picking from MayaHydra|
| [lib/mayaHydra/flowViewportAPIExamples/footPrintNode](https://github.com/Autodesk/maya-hydra/tree/dev/lib/mayaHydra/flowViewportAPIExamples/footPrintNode) | Is a Maya node showing how to convert the Maya FootPrint node which is part of the samples from the Maya devkit. It shows how to create Hydra mesh primitives and deal with selection picking from MayaHydra|

## Note on performance and scene indices
When you add custom Hydra primitives through a scene index or add filtering scene indices, you should be aware that the performance of the viewport can be impacted. 
It is usually better not to add too many scene indices. <BR>
As an example, if you want to add 1 000 custom primitives, you should create a single scene index and create all the custom primitives in it. <B>You should avoid having 1 scene index per primitive</B>.<BR>
A test we did was to create 10 000 Flow viewport API locator nodes which for each node creates 1 scene index for adding the grid of cubes and 1 filtering scene index to hide primitives with more than 10 000 vertices.<BR>
As a result, we had 20 000 scene indices and the performance was impacted. The viewport was not responding any more. <BR>

Here is an example on performance and scene indices : we created N Flow viewportAPI locator nodes in Maya's VP2, measure the time when switching to Hydra Storm :

| N Nodes       | One scene index per node              |One scene index for all nodes                                                                                  |
|-------------  |--------------------------             |-----------------------------  |
|100            | 100 scene indices, time : <B>0.18</B> sec         | 1 scene index, time : <B>0.04</B> sec|
|1 000           | 1 000 scene indices, time : <B>181.5</B> sec        | 1 scene index, time : <B>0.06</B> sec|
|10 000          | 10 000 scene indices, time : <B>Aborted after 1 Hour</B> | 1 scene index, time : <B>0.27</B> sec|

## Note on UFE implementation and scene index
A potential issue is that if a plugin both implements [UFE](https://git.autodesk.com/media-and-entertainment/ufe) and registers a custom Hydra scene index to add Hydra primitives using the Flow viewport toolkit, items that are represented in both will get added twice to the viewport, once by MayaHydra converting the UFE item, and once by the plugin through its custom scene index.<BR>
So please keep in mind to only implement one or the other.
